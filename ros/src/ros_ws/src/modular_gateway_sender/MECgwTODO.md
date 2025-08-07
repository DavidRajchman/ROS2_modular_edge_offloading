# MEC Gateway Implementation TODO List

## New Files to Create

- [x] **`src/mec_main.cpp`**
  - ✅ Create new main executable for MEC gateway - COMPLETED
  - ✅ Similar to current VHC main but with component_type set to "M" - COMPLETED
  - ✅ Use different default component name ("mec_gateway") - COMPLETED
  - ✅ Call setup_logging("MEC") instead of setup_logging("VHC") - COMPLETED
  - ✅ Different default parameters (group_id: 70, id_in_group: 1, port: 7501) - COMPLETED

## Existing Files to Modify

- [x] **`include/modular_gateway_sender/gateway_controller.hpp` & `src/gateway_controller.cpp`**
  - ✅ Add component type detection in constructor (read `identity.component_type` parameter) - Already implemented
  - ✅ Store component type as member variable (`component_type_`) - Already implemented  
  - ✅ Conditionally create ROS offloading services only when component_type == "V" - COMPLETED
  - ✅ Conditionally create ROS termination services only when component_type == "V" - COMPLETED
  - ✅ Modify `control_thread_func()` OPERATIONAL state to only process offloading request queue for VHC type - COMPLETED
  - [x] Modify `on_bridge_session_approved()` to handle MEC case where no local session exists (extract task_id from payload instead of session map)
  - [x] Adjust handler creation logic to allow for both publisher and subscriber roles, based on task type (input/output topic orientation) MEC will publish the data for processing and then subscribe to the result topic which will then send the data
  - example task below - STRING TEST PIPELINE input 201, output 202 which means, VHC will subscribe to 201 and publish to 202, MEC will subscribe to 202 and publish to 201 (REMEMBER the publisher and subscriber reference the ROS behaviour, not network behaviour)

  here is the example json part for string test pipeline task:

```json
      {
      "task_id": 3,
      "task_name": "STRING_TEST_PIPELINE",
      "input_message_types": [201],
      "output_message_types": [202]
    },
```

- [x] **`include/modular_gateway_sender/bridge_cp_client.hpp` & `src/bridge_cp_client.cpp`**
  - [x] Modify `handle_received_message()` to handle unsolicited SESSION_APPROVED messages for MEC
  - [x] Add logic to extract task_id from SESSION_APPROVED payload when no matching request_id exists in pending_acks
  - [x] Ensure MEC can receive and process SESSION_APPROVED without having sent an OFFLOAD_REQUEST

- [ ] **`src/handler_factory.cpp`**
  - No changes needed - handlers work the same for both VHC and MEC

- [x] **`include/modular_gateway_sender/message_handler_base.hpp` & `src/message_handler_base.cpp`**
  - ✅ Handler mode configuration (SUBSCRIBER_ONLY, PUBLISHER_ONLY, BOTH) already implemented
  - ✅ Method to configure handlers differently for VHC vs MEC roles (input/output topic orientation) already implemented

- [ ] **Individual Handler Files (`src/handlers/*.cpp`)**
  - No immediate changes needed - same handlers used for both VHC and MEC
  - Future enhancement: configure subscription/publication behavior based on component type

- [ ] **`src/discovery_client.cpp`**
  - No changes needed - uses component_type from GatewayController

- [ ] **`src/ros_gateway.cpp`**
  - No changes needed - data plane behavior identical for VHC and MEC

- [ ] **Transport Files (`src/transport/*.cpp`)**
  - No changes needed - transport layer identical for VHC and MEC

- [ ] **`src/logging_setup.cpp`**
  - Already supports different component types via parameter

## Configuration Changes

- [x] **CMakeLists.txt**
  - ✅ Add new executable target for `mec_main.cpp` - COMPLETED
  - ✅ Ensure same dependencies as VHC executable - COMPLETED

- [ ] **Launch Files (if any)**
  - Create MEC-specific launch files with component_type parameter set to "M"
  - Different default parameters (component_name, group_id, id_in_group)

## Testing and Validation

- [ ] **Test MEC Registration**
  - Verify MEC registers with DiscoveryService as type "M"
  - Verify MEC connects to Bridge CP successfully

- [ ] **Test MEC Session Assignment**
  - Verify MEC receives SESSION_APPROVED from Bridge without prior request
  - Verify MEC creates handlers based on task_id from SESSION_APPROVED payload

- [ ] **Test Data Plane Flow**
  - Verify bidirectional data flow between VHC and MEC through Bridge
  - Verify handler topic subscription/publication works correctly

- [ ] **Test Session Lifecycle**
  - Verify MEC handles session termination properly
  - Verify MEC cleans up handlers when session ends

## Notes

- The core architecture supports both VHC and MEC with minimal changes
- Most changes are conditional logic based on component_type parameter
- Data plane and transport layers require no modifications
- Handler creation and topic management logic is shared between VHC and MEC
