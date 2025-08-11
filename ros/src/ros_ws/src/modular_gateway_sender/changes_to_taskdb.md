Summary of your request
- Replace the mock, hardcoded task database with a real parser of the global configuration JSON and populate tasks from it using the existing nlohmann::json.
- Simplify and centralize handler setup so that tasks and handlers are configured in only two places: a task database (populated from the JSON) and a handler registry/factory. Remove duplication or ad-hoc configuration scattered elsewhere.
- Use message types from message_header.hpp (numeric enum values) as the canonical identifiers in the config and in the code.

Implementation proposal (no code yet)

1) Parse the real global config and fill the task database

Where and when to parse
- parse after successful discovery.
- The registration response message includes the global config json which needs to be parsed and loaded.
- NO RUNTIME changes - after the config is parsed after discovery, it will not be changed during the runtime of the system. Changes to global config will require app restart
- at startup no global config is present. Thus offloading requests cannot be handled (the current version of this app only starts handling them after the state changes to CONNECTED which is after the global config is available)

here is the full registration response struct available in the discovery protocol library
```cpp
struct RegistrationResponse {
    ResponseCode responseCode;
    uint8_t assignedGroupId;
    uint8_t assignedIdInGroup;
    std::string connectionTargetType;
    std::string connectionTargetAddress;
    std::string connectionTargetPort;
    uint16_t connectionTargetId;
    std::string configJson;
    std::string detectedClientAddress;  
    std::string humanReadableMessage;
};
```

Data model changes
- Change the internal task database to use numeric message types from the enum in include/modular_gateway_sender/message_header.hpp as first-class citizens:
  - Keep task_id as string key in `GatewayController::task_database_` to avoid widespread changes, but store the message types as vectors of MessageType instead of strings. Example:
    struct TaskDetails {
      std::string task_name;
      std::vector<MessageType> input_types;
      std::vector<MessageType> output_types;
      std::vector<std::string> required_handlers; // kept only for backward-compat
    };
- Rationale: the global config uses numeric IDs; using MessageType avoids stringly-typed mismatches and eliminates an extra mapping layer.

Parsing logic
- Add a helper inside `GatewayController` (or a tiny GlobalConfigManager) to:
  - Parse nlohmann::json.
  - For each entry in available_tasks:
    - Read task_id (int) and normalize to string key (std::to_string) to fit current map key type.
    - Store task_name.
    - Map each numeric message type (e.g., 1, 11, 201, 202) to the enum MessageType via a small converter:
      - If unknown ID, log and skip.
    - Populate TaskDetails.input_types and TaskDetails.output_types with the enum values.
- Default session timeout and max_concurrent_sessions can be stored in controller members for future use (e.g., terminating idle sessions), but are not strictly required for first step.

Validation and diagnostics
- On load, log how many tasks were parsed, and per-task print a concise summary, e.g. “task 3 STRING_TEST_PIPELINE: 1 input, 1 output”.
- Hard stop or soft warnings: if a task references a message type not present in our handler registry (see section 2), log an error but keep other tasks.

2) Simplify and centralize the handler setup

Goals
- Make handler selection derive purely from:
  - The task database (which now carries input/output MessageType vectors).
  - A single handler registry (factory) that maps MessageType to a concrete handler class and its defaults.
- Remove other places where handler wiring is duplicated or hardcoded (strings in the task DB, ad-hoc topic names in multiple spots).

HandlerFactory as the single registry
- Change `HandlerFactory` to be keyed by MessageType instead of arbitrary strings. It will provide:
  - A registry: MessageType -> CreatorFunc(RosGateway*, Node) that returns a concrete handler instance.
  - A default topic/name policy per MessageType. Options:
    - Centralize default topic names in the factory (recommended), e.g. map<MessageType, std::string> default_topics_. Example defaults:
      - STRING -> "test/string"
      - smLASERSCAN -> "test/scan"
      - STRING_TEST_INPUT -> "test/string_input"
      - STRING_TEST_RESULT -> "test/string_result"
    - Allow ROS parameters to override these per handler (handlers can still declare their param to override; if not set, consult the centralized default).
- Rationale: today topic defaults live inside each handler class with inconsistent defaults (e.g. “test_result_topic” vs “test/string_result”), which likely contributed to the “no packets on the wire” confusion. Centralizing the defaults eliminates drift.

Handler lifecycle and idempotency
- Keep the one-instance-per-type model already implied by `RosGateway::register_handler`, which stores handlers by name. With factory keyed by MessageType, ensure each handler class exposes a stable name so duplicate registration is avoided.
- Add a method in HandlerFactory: get_or_create(MessageType) that either returns an existing registered handler (if already created) or constructs a new one.

Modes and who-does-what
- Decide handler modes purely from component role and the task DB:
  - For VHC:
    - Subscribe to inputs, publish outputs
  - For MEC:
    - Publish inputs, subscribe to outputs
- This logic already exists in `GatewayController::on_session_approved` but currently relies on string-based handler lists. Replace it to iterate over TaskDetails.input_types and output_types (MessageType) and call HandlerFactory to fetch handlers; then call `MessageHandlerBase::configure_handler_mode` with the correct mode.

Remove duplicated configuration surfaces
- Remove the string-based handler identifiers in the task database and replace them with MessageType vectors:
  - Deprecate TaskDetails.required_handlers for new tasks. Keep it as a fallback for legacy/temporary compatibility, but do not populate it from the JSON.
- Limit the handler type registration to HandlerFactory:
  - The factory will be the only place mapping “which MessageType is handled by which class.”
  - Handlers continue to encapsulate their ROS pub/sub and call back into `RosGateway::send_message` for network transmission.
- Ensure no other code path constructs handler lists (only HandlerFactory and the JSON-driven DB do).

Topic naming consistency
- Centralize defaults in HandlerFactory and make each handler consume them (or still accept a per-handler ROS parameter override). This resolves the likely mismatch between “test/string_result” and “test_result_topic”.
- Provide a single log line on activation summarizing the resolved topic per handler type to simplify telemetry.

How this fixes “handler created but nothing on the network”
- With per-type default topics centralized and consistent, and with the correct mode assignment (VHC subscribes to STRING_TEST_INPUT and publishes STRING_TEST_RESULT; MEC does the inverse), messages published on the ROS topics will be processed by exactly one handler instance per type and forwarded via `RosGateway::send_message`.
- If the Bridge expects specific topic strings embedded in the header, both sides will now agree on the topic name for the given MessageType, eliminating silent drops due to topic name mismatches.

Small API shifts (illustrative snippets)
- Enum mapping utility:
  - MessageType type_from_id(uint8_t id) { switch(id) { case 1: return MessageType::STRING; case 11: return MessageType::smLASERSCAN; case 201: return MessageType::STRING_TEST_INPUT; case 202: return MessageType::STRING_TEST_RESULT; default: throw; } }
- HandlerFactory interface (conceptual):
  - std::shared_ptr<MessageHandlerBase> get_or_create(MessageType type);
  - std::string default_topic(MessageType type, const std::string& role) const; // role can be "V-input"/"V-output"/"M-input"/"M-output" if you want directional names
- GatewayController uses only:
  - task_database_[task_id].input_types and output_types
  - For each type, fetch handler and set mode based on role

Migration plan
- Phase 1 (non-breaking):
  - Add JSON parsing and build the new DB fields (input_types/output_types).
  - Keep existing string-based fields as fallback; if JSON is present, prefer MessageType vectors; else, use existing strings.
  - HandlerFactory: add MessageType-based registry alongside the current string-based entries.
  - GatewayController::on_session_approved: prefer the MessageType vectors if present; fallback to legacy strings otherwise.
- Phase 2 (cleanup):
  - Remove legacy string handler lists from the DB (required_handlers).
  - Remove string-based creator map entries from HandlerFactory.
  - Consolidate topic defaults into HandlerFactory and align handler classes to use those defaults or param overrides.

Open questions for you
- Source of the global config: should I implement reading from a ROS param (inline JSON or path), or will DiscoveryService send CONFIGURATION? If DiscoveryService, what message code and payload format do we receive on the CP?
ANSWER - DiscoveryService will send the global config in its response message disc, req. in the document above the exact cpp struct with the parsed message that includes the entire json can be found
- Preferred default topics per MessageType, to harden interop:
  - STRING (1): ?
  - smLASERSCAN (11): “scan” or “test/scan”?
  - STRING_TEST_INPUT (201): “test/string_input”
  - STRING_TEST_RESULT (202): “test/string_result”
ANSWER - DO NOT OVERIDE THE default declared parameter topics for message types, if needed they will be overiden in launch files.
- Do you want different default topics for VHC vs MEC roles, or a single canonical topic per type across the system?
ANSWER - Topics are the same for MEC and VHC since they are copies of each other, just one is in the cloud.

Impact summary
- Task initialization: real JSON drives the task database; no more hardcoded entries in `GatewayController`.
- Handler setup: created only via a single registry keyed by MessageType in `HandlerFactory`; modes derived from component role and task DB; topics centralized to avoid drift.
- Backward compatibility: preserved during transition; can remove legacy paths after validation.