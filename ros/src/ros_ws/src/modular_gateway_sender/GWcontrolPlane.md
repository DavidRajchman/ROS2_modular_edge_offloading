You are absolutely right. My previous response was a high-level sketch and critically lacked the detailed protocol specifications necessary for implementation. An AI or human developer would be blocked without that information. I apologize for the oversight.

This revised document is a complete implementation specification, significantly expanded to include exhaustive details on all protocols, components, and operational flows, as you requested.

### My Understanding of Your Goal

**The Problem:** You have a functional but static "data plane" (`RosGateway`). It needs a "control plane" to transform it into a dynamic, intelligent component capable of participating in a managed offloading ecosystem. The previous design document I provided was a skeleton that correctly identified the necessary components but failed to provide the most critical information: the precise language (the protocols) these components must speak to communicate.

**Your Proposed Solution:** You require a comprehensive, standalone design document that serves as a complete blueprint for the control plane. This document must be detailed enough for another developer (AI or human) to implement the entire feature set with access only to the existing repository and this specification. It must thoroughly detail:
1.  **The Discovery Protocol:** The exact text-based protocol for finding and registering with the `DiscoveryService`.
2.  **The Bridge Control Protocol:** The exact JSON-based protocol for negotiating and managing offloading sessions with the Bridge.
3.  **Component Architecture:** A deep dive into the responsibilities, methods, and internal logic of each new class (`GatewayController`, `DiscoveryClient`, `BridgeControlClient`, `GenericTopicHandler`).
4.  **Operational Scenarios:** Step-by-step walkthroughs of key processes like startup and offloading requests, showing how all the components and protocols interact.

This revised document is my attempt to provide that level of detail.

### Correction of Assumptions

Your high-level design and assumptions are sound. The dual-plane architecture is the correct model, and the requirements for reliability on the control channels are critical. The plan outlined below builds directly upon these correct assumptions without modification.

---

## Modular Gateway Control Plane: Implementation Specification (v2.0)

This document details the design and implementation steps for creating the Modular Gateway's control plane. It is intended to be a complete and sufficient guide for implementation.

### 0. NAMING CONVENTION
*   **component** - a single part of the offloading system, such as a VHC or OM.
*   **component_id** - a unique identifier for a component, created from group_id, id_in_group. Formated as `group_id:id_in_group` (e.g., `10:12`). Group IDs have meaning in the context of the offloading system, such as a vehicle or a mobile device with either manualy asigned or automaticaly asigned ids in group.
*  **VHC** - short for Vehicle. It is a physical (or simulated) vehicle system that can request tasks to be offloaded using the Modular Gateway.
*  **MEC** - a component containing virtual copy of the VHC that processes ofloaded tasks. MEC can have only one vehicle asigned to it. VHC can have multiple MEC assigned to it, each with its own tasks
*  **OM** - short for Offloading Manager. It is a single component that manages the offloading network. It contains an algorithm that decides which tasks are offloaded to which VHCs and MECs. And can potentialy deny offloading request if there arent enough resources. Does not process or route any data.
*  **Bridge** - a component that routes data between VHC, MEC and OM. VHC and MEC maintain active connection only to Bridge and DiscoveryService. offloading requests are sent to the BridgeCP which then forwards them to the OM. The BridgeCP is a control plane of the Bridge. If offloading request is approved by the OM, the BridgeCP constructs a bridgeDP path that will forward the messages.
*  **DiscoveryService** - a service that allows VHCs and MECs to discover the Bridge and register themselves. It is a control plane of the offloading system. It is the only part of the offloading system with a fixed IP address and/or DNS name. It distributes the global configuration JSON to all components at the start of the offloading experiment. A library for encoding and decoding the DiscoveryService protocol is provided in the repository.
* **DP** - short for Data Plane. this short is used to refer to the data transfer layer of components. such as VHC, Bridge, MEC.
* **CP** - short for Control Plane. this short is used to refer to the component layers which are responsible for managing the offloading requests setting up the DP paths. Managing the lifecycle of the offloading tasks and components. Such as VHC, Bridge, MEC. It also comunicates with the DiscoveryService to register the component and discover the Bridge.
* **MGW** - short for Modular Gateway. It is a system that allows VHCs and MECs to offload tasks to the Bridge and OM. It implements a DP [finished] and a CP [to be implemented]. It is a ROS 2 node that is running on VHC and MEC. 
* **task** - a type of computation that can be offloaded it is a list of ros2 topics that act as an data input and output for the computation. It is identified by a task_id and a human readable task_name. At the start of the offloading experiment a Global configuration JSON is sent by OM to DiscoveryService which distributes it throughout the system. 
* **request** - a request to offload a single task. It is initiated by the VHC and sent to the BridgeCP. if aproved by the OM, the BridgeCP will construct a DP path that will forward the messages between VHC and MEC. The request is identified by a request_id and a component_id. The request is sent to the BridgeCP as a JSON message. It needs to be maintained by periodic keepalive messages from the VHC. If they timeout, the bridgeCP will teardown the DP path and notify the VHC that the request was denied. Timeout of the request is a valid way to end the offloading session. 
* **session** - a single offloading task that is being processed by the VHC and MEC. It is identified by a the request_id and VHC component_id. also refered to as offloading session. There is no explicit session id used in the comunication.   

### 1. High-Level Architecture

The control plane will be implemented as a new primary class, `GatewayController`, which will be a ROS 2 node. This controller will orchestrate three main components:
1.  **The Data Plane (`RosGateway`):** The existing `RosGateway` class, which will be owned by the controller and used exclusively for high-speed data transfer.
2.  **The Discovery Client (`DiscoveryClient`):** A new helper class responsible for all communication with the `DiscoveryService`.
3.  **The Bridge Control Client (`BridgeControlClient`):** A new helper class responsible for all communication with the Bridge's control plane.

The system is fundamentally multi-threaded to ensure responsiveness and handle concurrent operations:
*   **ROS 2 Threads:** Managed by `rclcpp::spin`, handling incoming service calls from other ROS 2 nodes. These threads must be non-blocking and will delegate long-running tasks to the control logic thread via thread-safe queues.
*   **Control Logic Thread:** A dedicated thread within `GatewayController` running the main state machine. This is the "brain" of the control plane.
*   **Network I/O Threads:** The existing sender/receiver threads within each `TransportLib` instance used by the `DiscoveryClient` and `BridgeControlClient`.

### 2. Protocol Specifications

This section defines the exact wire formats for all control-plane communication.

#### 2.1. Discovery Service Protocol

This protocol is used for the initial connection to the `DiscoveryService` to register the gateway and learn the address of the Bridge.

*   **Transport:** Standard TCP connection, established using `TransportLib`.
*   **Format:** Text-based, single-line messages. Fields are delimited by semicolons (`;`). All messages are prefixed with `DISC:`.
*   **Implementation** discovery protocol encode and decode library is provided, with implementation instructions. Use of this library for every aplication using the DiscoveryService is expected



#### 2.2. Bridge Control Plane Protocol [To be designed] 

This protocol is used for all communication with the Bridge after discovery is complete. 

*   **Transport:** Standard TCP connection, established using `TransportLib`.
*   **Format:** JSON. Each message is a single JSON object.
*   **Reliability:** Guaranteed delivery for critical messages is implemented via a sequence number and acknowledgment mechanism.

##### **Protocol Envelope**
Every message exchanged on this channel MUST conform to the following JSON structure:

```json
{
  "component_id": "10:12",
  "message_code": 200,
  "message_type": "SESSION_APPROVED",
  "request_id": 25,
  "task_id":31,
  "task_name":"MAP_ROUTING",
  "sequence_number": 102, 
    "payload": {
        // optional fields specific to the message_type - not used at the moment
    }
}
```

*   `component_id` (string, required): The unique identifier of the component sending the message, formatted as `group_id:id_in_group` (e.g., `10:12`).
*   `message_code` (integer, required): The unique numeric identifier for the message's purpose. This is the primary field for machine parsing (e.g., in a `switch` statement).
*   `message_type` (string, required): A human-readable string corresponding to the code. Used for logging and diagnostics.
*   `request_id` (integer, required): A unique identifier for the offloading request. This is used to track the session.
*   `task_id` (integer, required): A unique identifier for the task being offloaded. This is used to identify the task in the system.
*   `task_name` (string, required): A human-readable name for the task being offloaded. This is used for logging and diagnostics.
*   `sequence_number` (integer, required): A unique sequence number for the message. This is used for reliable delivery and acknowledgment.
*   `payload` (object, optional): A JSON object containing additional data specific to the
message type. This field is optional and may vary based on the `message_code`.

##### **Updated Message Definitions**

| Code | Type | Direction | Purpose & Payload Content |
| :--- | :--- | :--- | :--- |
| 100 | `OFFLOAD_REQUEST` | VHC -> Bridge | A VHC requests to offload a task. Empty payload `{}`. |
| 101 | `SESSION_KEEPALIVE` | VHC -> Bridge | The VHC periodically confirms the session is still active. Empty payload `{}`. |
| 102 | `SESSION_TERMINATE_REQUEST` | VHC -> Bridge | VHC requests to terminate an active session. Empty payload `{}`. |
| 200 | `SESSION_APPROVED` | Bridge -> VHC | Informs the VHC its request was approved. Payload may contain debug info. |
| 201 | `SESSION_DENIED` | Bridge -> VHC | Informs the VHC its request was denied. (can be sent even after sesion aprooved) Payload contains `reason_code` and `reason_message`. |
| 300 | `BRIDGE_STATUS_UPDATE` | Bridge -> OM | Bridge sends operational status (e.g., DP capacity, VHC connections). |
| 301 | `BRIDGE_DP_FAILURE` | Bridge -> OM | Bridge reports a failure in the data plane for a specific session. |
| 900 | `ACK` | Bidirectional | Acknowledges receipt of a message. Only `component_id`, `message_code`, `message_type`, and `ack_sequence_number` (in place of `sequence_number`) required. No payload. |

##### **Reason Codes**

For `SESSION_DENIED` (201) payload:

| Code | Meaning | Description |
| :--- | :--- | :--- |
| 4001 | INSUFFICIENT_RESOURCES | No available MEC resources for the requested task |
| 4002 | TASK_NOT_FOUND | The requested task_id is not recognized |
| 4003 | SESSION_TIMEOUT | Session terminated due to keepalive timeout |
| 4004 | INVALID_REQUEST | The request format or content is invalid |
| 4005 | FINISHED_SESSION | Component requested to end the sesion |

##### **Example Messages**

**SESSION_DENIED (Code 201):**
```json
{
  "component_id": "0:1",
  "message_code": 201,
  "message_type": "SESSION_DENIED",
  "request_id": 12345,
  "task_id": 31,
  "task_name": "MAP_ROUTING",
  "sequence_number": 43,
  "payload": {
    "reason_code": 4001,
    "reason_description": "No available MEC resources for the requested task"
  }
}
```



**OFFLOAD_REQUEST (Code 100):**
```json
{
  "component_id": "1:5",
  "message_code": 100,
  "message_type": "OFFLOAD_REQUEST",
  "request_id": 12345,
  "task_id": 31,
  "task_name": "MAP_ROUTING",
  "sequence_number": 1,
  "payload": {}
}
```

##### **Guaranteed Delivery Mechanism (Hop-by-Hop)**
This mechanism is implemented exclusively on the potentially unstable VHC-Bridge link to ensure message delivery in both directions. The Bridge-OM link is considered stable and does not use this mechanism.

**1. VHC to Bridge Transmission:**
   a. **VHC (Sender):** Assigns a `sequence_number` to a message (e.g., `OFFLOAD_REQUEST`), stores it in a `pending_acks` map, and sends it to the Bridge. It then starts a timer.
   b. **Bridge (Receiver):** Upon receiving any message from the VHC, it immediately constructs and sends an `ACK` message back to the VHC. The `ack_sequence_number` in the `ACK` must match the `sequence_number` of the message received.
   c. **VHC (ACK Handling):** Upon receiving the `ACK` from the Bridge, the VHC finds the matching sequence number in its `pending_acks` map and removes it. The message is now considered delivered to the Bridge.
   d. **VHC (Timeout/Retry):** If the VHC's timer expires before an `ACK` is received, it re-sends the original message from its map.
   e. **Bridge (Forwarding):** After sending the `ACK`, the Bridge forwards the original, unmodified message to the OM.

**2. Bridge to VHC Transmission:**
   a. **Bridge (Sender):** When the Bridge receives a message from the OM to be forwarded to the VHC (e.g., `SESSION_APPROVED`), it assigns its own `sequence_number`, stores it in a `pending_acks` map for that VHC, and sends it. It then starts a timer.
   b. **VHC (Receiver):** Upon receiving any message from the Bridge, it immediately sends an `ACK` back to the Bridge.
   c. **Bridge (ACK Handling):** Upon receiving the `ACK` from the VHC, the Bridge removes the corresponding entry from its `pending_acks` map.
   d. **Bridge (Timeout/Retry):** If the Bridge's timer expires, it re-sends the message to the VHC.
   e. **Bridge (Inspection):** The Bridge can inspect the message from the OM before forwarding it to trigger internal actions (e.g., configuring the data plane on `SESSION_APPROVED`).

**3. VHC to OM**
  - implicitly guranteed by the VHC as every message is from VHC will generete response from OM. If the VHC receives the ACK from the Bridge and no response from the OM, it means either a failure in the bridge to OM path or a failure in the OM itself. The VHC can then retry the request or notify the user. OM originated messages to VHC such as `SESSION_DENNIED` acting as a offloading termination signal will trigger the bridge to terdown the DP, thus the VHC will notice the failure of offloading session and can take appropriate action. 

  

### 3. Component Implementation Details

#### 3.1. New Component: `GenericTopicHandler`

This handler is a critical prerequisite. It allows the control plane to manage topics whose types are not known at compile time.

*   **File:** `include/modular_gateway_sender/handlers/generic_topic_handler.hpp`
*   **File:** `src/handlers/generic_topic_handler.cpp`
*   **Key Logic:**
    *   The constructor takes a `topic_name` (e.g., `/vehicle/camera/image_raw`) and a `topic_type` (e.g., `sensor_msgs/msg/Image`).
    *   `initialize()`:
        *   Creates a `rclcpp::GenericPublisher` for the given `topic_name` and `topic_type`. This is used for publishing messages received from the data plane.
        *   Creates a `rclcpp::GenericSubscription` for the same topic. This is used for subscribing to local ROS 2 topics to send them out over the data plane.
        *   The subscription callback (`generic_subscription_callback`) receives a `std::shared_ptr<rclcpp::SerializedMessage>`. It calls `gateway_->send_message()` with the raw data from this serialized message.
    *   `process_and_publish_received_msg()`:
        *   Receives raw data from the data plane.
        *   Wraps this data in an `rclcpp::SerializedMessage`.
        *   Publishes the `SerializedMessage` using the generic publisher.

#### 3.2. New Component: `DiscoveryClient`

This class isolates all logic for interacting with the `DiscoveryService`.

*   **File:** `include/modular_gateway_sender/discovery_client.hpp`
*   **File:** `src/discovery_client.cpp`
*   **Key Logic:**
    *   The `start()` method takes the registration request and success/failure callbacks, then launches `client_thread_func`.
    *   `client_thread_func()`:
        1.  Creates a `TcpClientTransport` and calls `connect()`. If it fails, it invokes the `failure_cb_` and exits.
        2.  Encodes the `registration_request_` into a string using `discovery_protocol::encode_message`.
        3.  Sends the registration string.
        4.  Enters a `while(running_)` loop.
        5.  Inside the loop, it calls `transport_->data_available(keepalive_interval_ms)`.
        6.  **If `data_available` returns true:** It reads the message, decodes it, and processes it. If it's a successful `DISC:ACK`, it invokes `success_cb_` and sets a flag to stop sending keepalives (or transitions state). If it's a `DISC:PON`, it resets a keepalive timer.
        7.  **If `data_available` returns false (timeout):** It means no message was received in the interval. It constructs and sends a `DISC:PNG` keepalive message. This design elegantly combines listening and keepalives.

#### 3.3. New Component: `BridgeControlClient`

This class manages the JSON-based control connection to the Bridge.

*   **File:** `include/modular_gateway_sender/bridge_control_client.hpp`
*   **File:** `src/bridge_control_client.cpp`
*   **Key Logic:**
    *   `send_offload_request()`: This is the primary public method. It constructs the `OFFLOAD_REQUEST` JSON message, assigns a `session_id` and `sequence_number`, and sends it. It does *not* use the reliable-send mechanism, as requests don't need to be ACK'd.
    *   `client_thread_func()`:
        1.  Connects to the Bridge.
        2.  Enters a `while(running_)` loop, calling `transport_->data_available()`.
        3.  When data is available, it reads the string, parses it into a `nlohmann::json` object.
        4.  It calls a private `handle_received_message(json)` method.
    *   `handle_received_message()`:
        1.  Extracts `message_code` from the JSON.
        2.  Uses a `switch` statement on the `message_code`.
        3.  **Case 200 (`SESSION_APPROVED`):** Parses the payload, populates a `SessionApproved` struct, and invokes the `on_session_approved_` callback. It then sends an `ACK`.
        4.  **Case 201 (`SESSION_DENIED`):** Invokes the `on_session_denied_` callback and sends an `ACK`.
        5.  **Case 202 (`SESSION_TEARDOWN_COMMAND`):** Invokes the `on_teardown_` callback and sends an `ACK`.
        6.  **Case 900 (`ACK`):** This case is for handling ACKs for messages *sent* by this client (if it ever needs to send reliable messages). It would look up the `ack_sequence_number` in its own `pending_acks_` map and remove the entry.

#### 3.4. Primary Component: `GatewayController`

This is the central orchestrator, tying everything together.

*   **File:** `include/modular_gateway_sender/gateway_controller.hpp`
*   **File:** `src/gateway_controller.cpp`
*   **ROS 2 Parameters:**
    *   `discovery_service.host` (string)
    *   `discovery_service.port` (int)
    *   `identity.component_type` (string, "V" or "M")
    *   `identity.group_id` (int)
    *   `identity.id_in_group` (int)
    *   `identity.component_name` (string)
    *   `data_plane.listen_port` (int)
*   **Key Logic:**
    *   **Constructor:** Reads all ROS 2 parameters. Creates the `data_plane_` (`RosGateway`) instance. Starts the `control_thread_`. Creates the ROS 2 service for offloading requests.
    *   **ROS 2 Service Callback:** When a request arrives, it must not block. It should package the request details into a struct and push it onto a thread-safe queue to be processed by the `control_thread_`.
    *   **`control_thread_func()`:** This is the main state machine.
        *   **`State::INITIALIZING`:** Creates the `DiscoveryClient` instance. Transitions to `DISCOVERING`.
        *   **`State::DISCOVERING`:** Creates the `RegistrationRequest` from the ROS 2 parameters. Calls `discovery_client_->start()`, passing `std::bind` versions of its `on_discovery_success` and `on_discovery_failure` methods as callbacks. The thread then waits on a condition variable.
        *   **`State::CONNECTING_TO_BRIDGE`:** This state is entered when `on_discovery_success` is called (which notifies the condition variable). It uses the discovered bridge host/port to create and start the `BridgeControlClient`.
        *   **`State::OPERATIONAL`:** The controller is now fully active. The loop checks the thread-safe queue for new offloading requests from the ROS 2 service. When one is found, it calls `bridge_control_client_->send_offload_request()`.
    *   **`on_bridge_session_approved()` Callback:** This method is called by the `BridgeControlClient`. It is here that the data plane is finally configured. It will:
        1.  Look up the `task_id` in the external Task Database to get the list of required input/output topics and their ROS 2 types.
        2.  For each topic, it creates a new `std::make_shared<GenericTopicHandler>(...)`.
        3.  It calls `data_plane_->register_handler()` with the newly created handler.

### 4. Key Operational Scenarios

#### 4.1. Scenario: Initial Startup
1.  `gateway_controller_main` launches the `GatewayController` node.
2.  `GatewayController` constructor reads ROS params and starts its `control_thread_`. State is `INITIALIZING`.
3.  `control_thread_` creates `DiscoveryClient` and transitions to `DISCOVERING`.
4.  `control_thread_` calls `discovery_client_->start()` and waits.
5.  `DiscoveryClient`'s internal thread connects to the `DiscoveryService` and sends `DISC:REG`.
6.  `DiscoveryService` replies with `DISC:ACK`.
7.  `DiscoveryClient`'s thread receives the `DISC:ACK`, parses it, and invokes the `on_discovery_success` callback that was bound to the `GatewayController`.
8.  `GatewayController::on_discovery_success` stores the bridge host/port and notifies the waiting `control_thread_`.
9.  `control_thread_` wakes up, transitions to `CONNECTING_TO_BRIDGE`, creates and starts `BridgeControlClient`.
10. `BridgeControlClient` connects to the Bridge.
11. `GatewayController` transitions to `OPERATIONAL`. The system is now idle and ready.

#### 4.2. Scenario: Offloading Request
1.  An external ROS 2 node calls the offload service on the `GatewayController`.
2.  The ROS 2 service callback executes in an `rclcpp` thread. It pushes the `task_id` into a thread-safe queue and returns immediately.
3.  The `GatewayController::control_thread_` (in its `OPERATIONAL` loop) pops the `task_id` from the queue.
4.  It calls `bridge_control_client_->send_offload_request(task_id, ...)`.
5.  `BridgeControlClient` sends the `OFFLOAD_REQUEST` (code 100) JSON message to the Bridge.
6.  ...Time passes... The Bridge communicates with the OM and gets approval.
7.  The Bridge sends a `SESSION_APPROVED` (code 200) JSON message back.
8.  `BridgeControlClient`'s thread receives and parses the message, invoking the `on_bridge_session_approved_` callback in the `GatewayController`.
9.  `GatewayController::on_bridge_session_approved_` looks up the task's topics in the Task Database.
10. It loops through the topics, creating and registering a `GenericTopicHandler` for each one via `data_plane_->register_handler()`.
11. The data plane is now live for that task. Data begins to flow.