## 0. NAMING CONVENTION
Core terms actually used in the codebase (see referenced files):
* **component** – Any participating runtime (VHC, MEC, OM, Bridge, DiscoveryService).
* **component_id** – Concatenation `group_id:id_in_group` (e.g. `60:5`) built in `gateway_controller.cpp` from parameters `identity.group_id` & `identity.id_in_group`.
* **VHC** – Vehicle host component (identity.component_type="V"). Requests offloading via service `request_offloading`.
* **MEC** – Mirror execution component (identity.component_type="M"). Receives unsolicited SESSION_APPROVED and processes data.
* **OM** – Offloading Manager (external, not implemented here) that decides approvals.
* **Bridge / BridgeCP / BridgeDP** – Bridge Control Plane (JSON session/control messages) & Data Plane (binary framed payload). The gateway connects CP via `BridgeCpClient` and DP via `TcpServerTransport`.
* **DiscoveryService** – Entry point supplying `RegistrationResponse` with `configJson` consumed in `GatewayController::on_discovery_success`.
* **DP** – Data Plane (binary framed messages; see `message_header.hpp`).
* **CP** – Control Plane (session negotiation & keepalives; see `bridge_cp_client.cpp`).
* **MGW** – Modular Gateway node (`gateway_controller` executable for VHC; `mec_gateway` for MEC) orchestrating handlers + transports.
* **task** – Offloadable unit defined in global config JSON (fields: `task_id`, `task_name`, `input_message_types`, `output_message_types`). Parsed into internal TaskDetails in `gateway_controller.cpp`.
* **request** – OFFLOAD_REQUEST initiated by VHC (see `BridgeCpClient::send_offload_request`). Identified by `request_id` (local monotonic counter) + component_id.
* **session** – Active offloaded task lifecycle between VHC & MEC. Internally tracked in `active_sessions_` keyed by `request_id` (no separate session ID on the wire).
* **MessageType** – Numeric enumeration (`message_header.hpp`) used for both global config and binary header type byte.
* **handler** – Concrete subclass of `MessageHandlerBase` bridging a ROS 2 topic to the MGW DP (e.g. `StringTestInputHandler`).
* **handler mode** – One of SUBSCRIBER_ONLY / PUBLISHER_ONLY / BOTH (see `message_handler_base.hpp`, configured per role in `on_session_approved`).
* **task database** – In-memory map `<task_id, TaskDetails>` populated once from `configJson` (see `load_global_config_from_json`).
* **global configuration JSON** – Distributed once via Discovery; authoritative catalog of tasks.
* **binary log** – Persistent logging output (CppLogging format) including per-source file prefixes; used for diagnostics and research analysis.
* **keepalive** – CP ping/pong for session liveness (Discovery keepalive) & session-level keepalives (Bridge CP).
* **ACK** – Control-plane acknowledgement for reliable message sequences (managed in `BridgeCpClient`).

---

## 1. INTRODUCTION & SCOPE
This user guide targets researchers/developers extending or using the Modular Gateway (MGW) for ROS 2–based task offloading between a Vehicle Host Component (VHC) and a Mirror Execution Component (MEC). It focuses on: building, configuring, adding message types & handlers, initiating offloading, and diagnosing runtime behavior. Deep internal design (state machines, protocol encoding details) is covered in the separate technical documentation.

MGW responsibilities (current state):
* Discover Bridge endpoints via DiscoveryService.
* Load authoritative global config JSON describing available tasks.
* Manage control-plane sessions (request, approval, keepalive, teardown) via `BridgeCpClient`.
* Forward ROS 2 topic payloads over a binary data plane with per-message framing (see `message_header.hpp`).

Not in scope here: OM decision logic, Bridge internals, DiscoveryService operations (each has its own guide).

---

## 2. HIGH-LEVEL ARCHITECTURE (CONCISE)
Sequence (VHC perspective):
1. `gateway_controller` node starts (`GatewayController` class) → state INITIALIZING → DISCOVERING.
2. `DiscoveryClient` registers; receives `RegistrationResponse` containing `configJson` (task catalog).
3. `GatewayController::load_global_config_from_json` populates TaskDetails.
4. Transition to CONNECTING_TO_BRIDGE → `BridgeCpClient` established → WAITING_FOR_DP_CONNECTION → OPERATIONAL after DP confirmation.
5. Research application (e.g. `vhc_node.py`) calls ROS service `request_offloading` (task_id string) → MGW sends OFFLOAD_REQUEST via Bridge CP.
6. On SESSION_APPROVED, MGW activates handlers for listed input/output `MessageType`s with role-based modes.
7. Handlers forward ROS topic data to DP; remote counterpart publishes into its ROS graph.
8. Session keepalives maintain liveness until termination or timeout.

Key components & files:
* Controller: `src/modular_gateway_sender/src/gateway_controller.cpp`
* Discovery: `src/modular_gateway_sender/src/discovery_client.cpp`
* Bridge CP client: `src/modular_gateway_sender/src/bridge_cp_client.cpp`
* Data plane transport: `src/modular_gateway_sender/src/transport/tcp_server_transport.cpp`
* Handler factory: `src/modular_gateway_sender/src/handler_factory.cpp`
* Message framing: `src/modular_gateway_sender/include/modular_gateway_sender/message_header.hpp`

---

## 3. BUILD & INSTALLATION
Prerequisites: ROS 2 (Jazzy), ament/colcon toolchain, C++17 compiler, Python 3 (for test nodes). The package `modular_gateway_sender` provides both VHC and MEC executables.

Basic steps (conceptual – run inside workspace root):
1. Source ROS 2 environment.
2. Place or update global workspace dependencies (Discovery protocol library already included under `discovery_protocol`).
3. Build: `colcon build --packages-select modular_gateway_sender offloading_latency_test_loopback`.
4. Source overlay: `source install/setup.bash`.
5. use ros2 run to start the application (e.g. `ros2 run modular_gateway_sender gateway_controller`).

Node names:
* VHC: `gateway_controller`
* MEC: `mec_gateway`

---

## 4. RUNTIME CONFIGURATION
Parameters (declared in `gateway_controller.cpp`):
* `discovery_service.host` / `discovery_service.port`
* `identity.component_type` ("V" or "M")
* `identity.component_name`
* `identity.group_id`, `identity.id_in_group`
* `data_plane.listen_port`
* Per-handler topic override parameters (declared in each handler, e.g. `string_test_input_handler.topic`).

Global Config JSON (delivered in `RegistrationResponse.configJson`):
Example task (subset):
```json
{
	"task_id": 3,
	"task_name": "STRING_TEST_PIPELINE",
	"input_message_types": [201],
	"output_message_types": [202]
}
```
Loaded once at discovery success → immutable for remainder of process lifetime (changes require restart).

Task Database Population:
* Performed by `GatewayController::load_global_config_from_json`.
* Maps numeric IDs to `MessageType` enum via `message_type_from_id`.
* Stores vectors: `input_types`, `output_types`.

Launch Overrides (example – see `launch/vch_launch.py` & `launch/mec_launch.py`):
* Provide consistent topic alignment across VHC/MEC by overriding handler parameters (e.g., `string_test_input_handler.topic`: `test_input_topic`).

Session Limits:
* `default_session_timeout` and `max_concurrent_sessions` fields parsed (future enforcement hooks present in controller members).

---

## 5. MESSAGE & PROTOCOL BASICS (USER ESSENTIALS)
MessageType Enumeration:
* Defined in `message_header.hpp` (e.g., STRING=1, smLASERSCAN=11, STRING_TEST_INPUT=201, STRING_TEST_RESULT=202).

Binary Data Plane Frame (simplified user view):
* Magic (2 bytes) | Flags (1) | Type (1) | GroupID (1) | IdInGroup (1) | Payload Size (4) | TopicLen (1) | Topic (N) | Payload.
* Users usually only need to add new enum entries & rely on handlers for framing.

Control Plane Essentials:
* OFFLOAD_REQUEST / SESSION_APPROVED / SESSION_DENIED JSON messages exchanged via `BridgeCpClient`.
* Sequence numbers & ACK handling encapsulated (no manual intervention required for typical research extension tasks).

When Adding Types:
* Ensure unique numeric ID in allowed range (see existing block comments in `message_header.hpp`).
* Reference that ID in global config JSON task definitions.

---

## 6. ADDING A NEW MESSAGE TYPE
Steps overview (all must be consistent):
1. **Assign numeric ID** in `message_header.hpp` (respect ranges & avoid collisions).
2. **Update ID mapping** in `GatewayController::message_type_from_id` (switch-case addition).
3. **Create or adapt handler** if specialized processing is required (see Section 8).
4. **Register type in handler factory** (`handler_factory.cpp`: add to `mt_creator_map_`).
5. **Extend global config JSON** (add the type ID to `input_message_types` or `output_message_types` for selected task(s)).
6. Rebuild & verify log line: "Loaded X tasks ..." and handler activation lines referencing new numeric msgType.

Validation Tips:
* Use loopback test to confirm bidirectional flow.
* Check logs for "No handler registered for msgType" errors—indicates missing factory registration.

---

## 7. CREATING A NEW HANDLER
Handler Structure:
* Derive from `MessageHandlerBase` (header: `message_handler_base.hpp`).
* Implement: `initialize()`, `shutdown()`, `can_process_message_type()`, optionally `process_and_publish_received_msg()`.

Topic Parameter:
* Declare a parameter `<handler_name>.topic` in `initialize()` (pattern already used by `StringTestInputHandler`).
* Allow launch overrides for experiments without recompilation.

Direction (Mode) Selection:
* Mode assigned by controller at activation (`MessageHandlerBase::configure_handler_mode`).
	* VHC: subscribes to `input_types` / publishes `output_types`. 
	* MEC: publishes to `input_types` / subscribes to `output_types`.
    * Subscribe and publish are in terms of the ROS2 environment, not network

Sending Data:
* Inside subscription callback call `send_message(...)` with `MessageType` and raw buffer pointer (e.g., `msg->data.c_str()`).

Receiving Data:
* Override `process_and_publish_received_msg` to convert incoming payload to ROS message & publish.

Reuse vs New (WHAT EXISTS TODAY):
* There is currently NO templated or fully generic handler implementation in the codebase. All handlers are bespoke subclasses of `MessageHandlerBase` (e.g. `StringTestInputHandler`, `StringTestResultHandler`).
* "Reuse" therefore means: clone the minimal existing handler with the closest semantics (usually one of the string test handlers), then adapt:
	* Class / file names.
	* The `MessageType` constant(s) it advertises in `can_process_message_type()`.
	* The ROS message type used in subscription & publication.
	* Serialization / deserialization logic inside the subscription callback and `process_and_publish_received_msg`.
* Introduce a NEW handler only if the message flow demands different logic (e.g. transformation, aggregation, batching, compression) beyond a straightforward pass‑through copy of an existing pattern.
* Planned future improvement (NOT implemented): a registry-driven generic handler for simple pass‑through of standard ROS types to reduce class proliferation.

Logging:
* Use consistent prefix via CppLogging logger with file indicator (e.g., `string_test_input_handler.cpp:`). This is required for post-analysis scripts.

---

## 8. REQUESTING OFFLOADING (VHC WORKFLOW)
User Application Flow (see `offloading_latency_test_loopback/vhc_node.py`):
1. Wait for service `request_offloading` (created only on VHC MGW).
2. Call service with `task_id` (string form of numeric ID from global config).
3. Receive `request_id` in synchronous service response (local queuing only – actual approval asynchronous via Bridge CP).
4. Observe SESSION_APPROVED log; handlers for that task activate.
5. Publish input topic messages; MGW forwards them automatically.
6. Consume result messages on designated output topic.

Session Maintenance:
* Controller sends periodic keepalives; absence of approval or DP confirmation prevents session entry into OPERATIONAL.

Troubleshooting Checklist:
* If no result messages: verify topic overrides in launch, confirm logs for handler initialization & activation for msgTypes expected, ensure global config lists the task.

---

## 9. LOGGING & DIAGNOSTICS (INCLUDING BINARY LOG)
This section describes EXACTLY what exists for logging, how binary records are laid out, and how researchers convert them to human‑readable text for analysis.

### 9.1 Logging Framework In Use
* Library: CppLogging (external) – used via `logger_.Info/Warn/Error/Fatal/Debug`.
* Every emitted message string we provide already embeds the source file name prefix manually (e.g. `gateway_controller.cpp: Loaded 3 tasks ...`). This is deliberate so that after decoding we can filter by simple string matching without needing extra metadata.
* Log levels (enum values) encountered: NONE(0x00), FATAL(0x1F), ERROR(0x3F), WARN(0x7F), INFO(0x9F), DEBUG(0xBF), ALL(0xFF). Typical research runs use INFO.


### 9.2 Converting Binary Log To Text
Tool: `BinLogDecoder.py` (root of workspace). Supports default paths or explicit arguments.

Basic usage (inside workspace root):
```
python3 BinLogDecoder.py                      # uses defaults /home/ubuntu/ros_ws/V_binary.log -> decoded_log.txt
python3 BinLogDecoder.py V_binary.log         # specify input, default output path
python3 BinLogDecoder.py V_binary.log run1.txt # specify both input & output
```

Decoder output line format:
```
YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ [0xTHREAD] LEVEL LOGGER_NAME - fully_formatted_message
```
Example (illustrative):
```
2025-08-14T09:15:27.123456789Z [0x3A7F12] INFO  gateway - gateway_controller.cpp: Loaded 3 tasks from global config
```

### 9.3 Interpreting Messages (Structure Within Text)
Our emitted message strings intentionally begin with `<file>.cpp:` or similar. Pattern examples:
* `gateway_controller.cpp: STATE TRANSITION X->Y`
* `gateway_controller.cpp: Loaded N tasks ...`
* `bridge_cp_client.cpp: Sending OFFLOAD_REQUEST request_id=... task_id=...`
* `bridge_cp_client.cpp: SESSION_APPROVED request_id=... task_id=...`
* `handler_factory.cpp: Activating handler msgType=... mode=...`
* `string_test_input_handler.cpp: Forwarding payload size=...`

Use these stable prefixes for scripting (they should not be changed casually; downstream parsing may rely on them).

### 9.4 Common Diagnostic Paths Using Logs
* Missing results: Search decoded log for `SESSION_APPROVED` and subsequent handler activation; absence means CP issue.
* Silent drops: Look for `No handler processed message type` (INFO/WARN). If present, add factory registration or handler.
* Performance spikes: Measure deltas between first `Forwarding payload` and matching result publish log (if instrumented) to approximate round-trip latency.
* Session timeout: Inspect keepalive warnings (future: explicit timeout logs once enforcement added).


### 9.5 Failure Modes In Decoding
* `Truncated record` / `Incomplete data block`: Binary log corrupted or truncated mid-write; keep original, note corruption in metadata.
* `FORMAT_ERROR`: Mismatch between placeholders `{}` count and parsed arguments (should be rare – indicates code/log format drift or decoder desync).
* Unknown `arg_type`: Update `ARG_TYPES` mapping in `BinLogDecoder.py` if new CppLogging types were introduced.



---

End of user guide (research-focused). For deeper protocol/state machine details refer to the internal technical documentation.
