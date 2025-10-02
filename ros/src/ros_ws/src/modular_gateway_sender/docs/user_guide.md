## 0. NAMING CONVENTION
See [Appendix B in README.md](../../../../../../README.md#appendix-b---naming-convention-wip).
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
* `identity.component_type` ("V" or "M" for VHC/MEC)
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

## 9. CUSTOM VHC DATA IN OFFLOADING REQUESTS
The MGW supports optional application-specific data in offloading requests via the `vhc_data` parameter.

### 9.1 Service Interface
The `RequestOffloading` service accepts an optional `vhc_data` string field:
```cpp
string task_id      # Required: task identifier  
string vhc_data     # Optional: custom application data
```

### 9.2 Usage Examples
**Python VHC node:**
```python
request = RequestOffloading.Request()
request.task_id = "TASK_001"
request.vhc_data = "priority=high,location=37.7749,-122.4194,app=video_streaming"
```

**C++ VHC node:**
```cpp
request->task_id = "TASK_001";
request->vhc_data = "priority=high,location=37.7749,-122.4194,app=video_streaming";
```

### 9.3 JSON Payload to OM
When `vhc_data` is provided, it appears in the OFFLOAD_REQUEST payload:
```json
{
  "component_id": "bridge_001",
  "message_code": 100,
  "message_type": "OFFLOAD_REQUEST",
  "payload": {
    "request_id": 12345,
    "task_id": 67890,
    "task_name": "STRING_TEST_PIPELINE",
    "vhc_data": "priority=high,location=37.7749,-122.4194,app=video_streaming"
  }
}
```

**Notes:**
* Empty `vhc_data` values are omitted from JSON to keep messages clean.
* Backward compatible: existing VHC nodes continue working unchanged.
* Format: arbitrary string—structured data (CSV, JSON, key=value) recommended for parsing.

---

## 10. LOGGING & DIAGNOSTICS (INCLUDING BINARY LOG)
This section describes EXACTLY what exists for logging, how binary records are laid out, and how researchers convert them to human‑readable text for analysis.

### 10.1 Logging Framework In Use
* **Library**: CppLogging (external) – used via `logger_.Info/Warn/Error/Fatal/Debug`.
* **Performance considerations**: Binary logging causes less than 5μs latency per message; text logging can cause up to 200μs per message. Binary is recommended for research runs; text for direct debugging only.
* **Compilation mode selection**: MGW supports both binary and text logging modes, selectable at build time via CMake parameters (see section 9.6).
* **Message structure**: Every emitted message string embeds the source file name prefix manually (e.g. `gateway_controller.cpp: Loaded 3 tasks ...`). This is deliberate so that after decoding we can filter by simple string matching without needing extra metadata.
* **Log levels** (enum values): NONE(0x00), FATAL(0x1F), ERROR(0x3F), WARN(0x7F), INFO(0x9F), DEBUG(0xBF), ALL(0xFF). Typical research runs use INFO.

### 10.2 Converting Binary Log To Text
**Tool**: [BinLogDecoder.py](../../../../BinLogDecoder.py) (container workspace root). Uses known fmt library format specifiers to decode binary logs.


**Important**: Multiple [BinLogDecoder.py](../../../../BinLogDecoder.py) scripts exist in this repository  for different components. When updating format specifiers, modify all relevant decoders. In the MGW container, only this one is present.

Basic usage (inside workspace root):
```bash
python3 BinLogDecoder.py                          # uses defaults /home/ubuntu/ros_ws/V_binary.log -> decoded_log.txt
python3 BinLogDecoder.py V_binary.log             # specify input, default output path
python3 BinLogDecoder.py V_binary.log run1.txt    # specify both input & output
```

**Decoder behavior**: If format specifiers are not recognized, they will be ignored. Update the script's format mapping when new logging patterns are introduced.

Decoder output line format:
```
YYYY-MM-DDTHH:MM:SS.nnnnnnnnnZ [0xTHREAD] LEVEL LOGGER_NAME - fully_formatted_message
```

Example (illustrative):
```
2025-08-14T09:15:27.123456789Z [0x3A7F12] INFO  gateway - gateway_controller.cpp: Loaded 3 tasks from global config
```

### 10.3 Interpreting Messages (Structure Within Text)
Our emitted message strings intentionally begin with `<file>.cpp:` or similar. Pattern examples:
* `gateway_controller.cpp: STATE TRANSITION X->Y`
* `gateway_controller.cpp: Loaded N tasks ...`
* `bridge_cp_client.cpp: Sending OFFLOAD_REQUEST request_id=... task_id=...`



### 10.5 Failure Modes In Decoding
* **`Truncated record` / `Incomplete data block`**: Binary log corrupted or truncated mid-write; keep original, note corruption in metadata.
* **`FORMAT_ERROR`**: Mismatch between placeholders `{}` count and parsed arguments (should be rare – indicates code/log format drift or decoder desync).
* **Unknown `arg_type`**: Update `ARG_TYPES` mapping in `BinLogDecoder.py` if new CppLogging types were introduced.

### 10.6 Logging Mode Configuration
MGW supports compile-time selection between binary and text logging modes via CMake cache variables:

**Available modes**:
* `MGW_LOG_MODE=BINARY` (default, recommended for research): High-performance binary output requiring decoder
* `MGW_LOG_MODE=TEXT`: Direct console output for debugging (higher latency up to 500 us per message)

**Log level selection**:
* Preset: `MGW_LOG_LEVEL_PRESET=<NONE|FATAL|ERROR|WARN|INFO|DEBUG|ALL>`
* Direct: `MGW_LOG_LEVEL=0xBF` (when preset not specified)

**Build examples**:
```bash
# Research/production (binary logging, INFO level)
colcon build --packages-select modular_gateway_sender --cmake-args -DMGW_LOG_MODE=BINARY -DMGW_LOG_LEVEL_PRESET=INFO

# Development/debugging (text logging, DEBUG level)
colcon build --packages-select modular_gateway_sender --cmake-args -DMGW_LOG_MODE=TEXT -DMGW_LOG_LEVEL_PRESET=DEBUG

# Custom level using hex mask
colcon build --packages-select modular_gateway_sender --cmake-args -DMGW_LOG_MODE=BINARY -DMGW_LOG_LEVEL=0xBF
```

**Performance recommendation**: Use binary mode for everything, except for direct code debugging sessions where immediate console output is needed.



---

