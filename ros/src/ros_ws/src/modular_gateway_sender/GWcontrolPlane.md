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
* **session** - a single offloaded task that is being processed by the VHC and MEC. It is identified by a the request_id and VHC component_id. also refered to as offloading session. There is no explicit session id used in the comunication.   

### 0.5. Logging Configuration

To ensure consistent and manageable logging across all components, the project will use a centralized configuration strategy for the `CppLogging` library.

*   **Problem:** The initial implementation configured the global logger ("gateway") inside the `RosGateway` constructor. This is not ideal because:
    1.  It ties a global application concern (logging setup) to a specific class.
    2.  It makes it difficult to manage configuration as the application grows.
    3.  With the refactoring of `RosGateway` into a pure data plane component, it is no longer the correct place for this responsibility.

*   **Solution:** A dedicated, centralized function will be responsible for all logging configuration.
    1.  A new function, `void setup_logging(const std::string& component_type)`, will be created.
    2.  This function will be defined in new files: `src/logging_setup.cpp` and `include/modular_gateway_sender/logging_setup.hpp`.
    3.  It will contain all the logic for creating sinks (e.g., `FileAppender`), setting up asynchronous processing, and configuring the "gateway" logger via `CppLogging::Config::ConfigLogger`. The `component_type` parameter will be used to generate unique log file names (e.g., "VHC_binary.log", "MEC_binary.log").
    4.  This `setup_logging()` function **must** be the first call in the `main()` function of each executable (`gateway_VHC_main.cpp`, `gateway_MEC_main.cpp`, etc.).

*   **Usage in Classes:** All other classes (`GatewayController`, `RosGateway`, `DiscoveryClient`, etc.) will **not** perform any configuration. They will simply obtain an instance of the already-configured logger by calling `logger_ = CppLogging::Logger("gateway");`.


### 1. High-Level Architecture

The control plane will be implemented as a new primary class, `GatewayController`, which will be a ROS 2 node. This controller will orchestrate three main components:
1.  **The Data Plane (`RosGateway`):** The existing `RosGateway` class, which will be owned by the controller and used exclusively for high-speed data transfer.
2.  **The Discovery Client (`DiscoveryClient`):** A new helper class responsible for all communication with the `DiscoveryService`.
3.  **The Bridge Control Client (`BridgeControlClient`):** A new helper class responsible for all communication with the Bridge's control plane.

The system is fundamentally multi-threaded to ensure responsiveness and handle concurrent operations:
*   **ROS 2 Threads:** Managed by `rclcpp::spin`, handling incoming service calls from other ROS 2 nodes. These threads must be non-blocking and will delegate long-running tasks to the control logic thread via thread-safe queues.
*   **Control Logic Thread:** A dedicated thread within `GatewayController` running the main state machine. This is the "brain" of the control plane.
*   **Network I/O Threads:** The existing sender/receiver threads within each `TransportLib` instance used by the `DiscoveryClient` and `BridgeControlClient`.

### 1.5. Startup and Connection Sequence (CRITICAL)

To minimize latency, the Modular Gateway (MGW) MUST establish all necessary network connections during its initialization phase. The roles are distinct for each plane:
*   **Control Plane:** The MGW acts as a **TCP Client**.
*   **Data Plane:** The MGW acts as a **TCP Server**, listening on a pre-configured port.

The precise startup sequence is as follows:
1.  **MGW Initialization:** The `GatewayController` node starts and reads its configuration, including the port its Data Plane will listen on.
2.  **Data Plane Server Start:** The controller instantiates the `RosGateway` with a `TcpServerTransport`, which immediately begins listening for an incoming connection from the Bridge's Data Plane. A dedicated thread is launched to accept and manage this connection.
3.  **Discovery (Future Step):** The controller uses the `DiscoveryClient` to find the address of the Bridge's Control Plane. (For now, this is hardcoded).
4.  **Control Plane Connection:** The controller instantiates and connects the `BridgeControlClient` (as a client) to the Bridge's Control Plane (server).
5.  **Configuration Exchange:** Immediately upon connecting, the `BridgeControlClient` sends a registration message to the Bridge, providing the IP address and listening port of the MGW's Data Plane.
6.  **Bridge-Side Connection:** The Bridge's Control Plane receives this message and instructs its own Data Plane (as a client) to connect to the now-listening MGW Data Plane server.
7.  **Connection Confirmation:** Once the Bridge confirms its Data Plane has successfully connected, it sends a confirmation message back to the MGW over the control plane link.
8.  **Ready State:** With both control and data planes connected and stable, the `GatewayController` is now fully operational and ready to process offloading requests.

### 2. Protocol Specifications

This section defines the exact wire formats for all control-plane communication.

#### 2.1. Discovery Service Protocol

This protocol is used for the initial connection to the `DiscoveryService` to register the gateway and learn the address of the Bridge.

*   **Transport:** Standard TCP connection, established using `TransportLib`.
*   **Format:** Text-based, single-line messages. Fields are delimited by semicolons (`;`). All messages are prefixed with `DISC:`.
*   **Implementation** discovery protocol encode and decode library is provided, with implementation instructions. Use of this library for every aplication using the DiscoveryService is expected



#### 2.2. Bridge Control Plane Protocol [Revised]

This protocol is used for all communication with the Bridge after discovery is complete. It is a payload-centric design.

*   **Transport:** Standard TCP connection, established using `TransportLib`.
*   **Format:** JSON. Each message is a single JSON object.
*   **Reliability:** Guaranteed delivery for critical messages is implemented via a sequence number and acknowledgment mechanism.

##### **Protocol Envelope**
Every message exchanged on this channel MUST conform to the following universal JSON structure. All message-specific data is contained within the `payload` object.

```json
{
  "component_id": "10:12",
  "message_code": 100,
  "message_type": "OFFLOAD_REQUEST",
  "sequence_number": 102,
  "payload": {
    "request_id": 25,
    "task_id": 31,
    "task_name": "MAP_ROUTING"
  }
}
```

*   `component_id` (string, required): The unique identifier of the component sending the message, formatted as `group_id:id_in_group` (e.g., `10:12`).
*   `message_code` (integer, required): The unique numeric identifier for the message's purpose. This is the primary field for machine parsing.
*   `message_type` (string, required): A human-readable string corresponding to the code. Used for logging and diagnostics.
*   `sequence_number` (integer, required): A unique, monotonically increasing number for each message sent. Used for the reliability mechanism.
*   `payload` (object, required): A JSON object containing data specific to the `message_type`. The structure of this object varies.

##### **Updated Message Definitions & Payloads**

| Code | Type | Direction | Purpose & Payload Content |
| :--- | :--- | :--- | :--- |
| 100 | `OFFLOAD_REQUEST` | VHC -> Bridge | A VHC requests to offload a task. Payload: `{"request_id": int, "task_id": int, "task_name": str}` |
| 101 | `SESSION_KEEPALIVE` | VHC -> Bridge | The VHC periodically confirms the session is still active. Payload: `{"request_id": int}` |
| 102 | `SESSION_TERMINATE_REQUEST` | VHC -> Bridge | VHC requests to terminate an active session. Payload: `{"request_id": int}` |
| 103 | `DP_INFO` | VHC/MEC -> Bridge | Provides the address and port for the component's data plane. Payload: `{"dp_host": "str", "dp_port": int}`. |
| 200 | `SESSION_APPROVED` | Bridge -> VHC | Informs the VHC its request was approved. Payload: `{"request_id": int}` |
| 201 | `SESSION_DENIED` | Bridge -> VHC | Denies a request or terminates a session. Payload: `{"request_id": int, "reason_code": int, "reason_description": "str"}` |
| 202 | `DP_CONNECTION_CONFIRMED` | Bridge -> VHC/MEC | Confirms the Bridge's DP has connected to the component's DP. Empty payload `{}`. |
| 300 | `BRIDGE_STATUS_UPDATE` | Bridge -> OM | Bridge sends operational status. Payload specific to OM needs. |
| 301 | `BRIDGE_DP_FAILURE` | Bridge -> OM | Bridge reports a data plane failure. Payload: `{"request_id": int, "reason": "str"}` |
| 900 | `ACK` | Bidirectional | Acknowledges receipt of a message. Payload: `{"ack_sequence_number": int}`. |

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
  "sequence_number": 43,
  "payload": {
    "request_id": 12345,
    "reason_code": 4001,
    "reason_description": "No available MEC resources for the requested task"
  }
}
```

**DP_INFO (Code 103):**
```json
{
  "component_id": "1:5",
  "message_code": 103,
  "message_type": "DP_INFO",
  "sequence_number": 2,
  "payload": {
    "dp_host": "192.168.1.10",
    "dp_port": 7401
  }
}
```

##### **Guaranteed Delivery Mechanism (Hop-by-Hop)**
This mechanism is implemented exclusively on the potentially unstable VHC-Bridge link to ensure message delivery in both directions. The Bridge-OM link is considered stable and does not use this mechanism.

**1. VHC to Bridge Transmission:**
   a. **VHC (Sender):** Assigns a `sequence_number` to a message (e.g., `OFFLOAD_REQUEST`), stores it in a `pending_acks` map, and sends it to the Bridge. It then starts a timer.
   b. **Bridge (Receiver):** Upon receiving any message from the VHC, it immediately constructs and sends an `ACK` message back. The `ack_sequence_number` in the `ACK` payload must match the `sequence_number` of the message being acknowledged.
   c. **VHC (ACK Handling):** Upon receiving the `ACK`, the VHC extracts the `ack_sequence_number` from the payload, finds the matching sequence number in its `pending_acks` map, and removes it.
   d. **VHC (Timeout/Retry):** If the VHC's timer expires before an `ACK` is received, it re-sends the original message from its map.
   e. **Bridge (Forwarding):** After sending the `ACK`, the Bridge forwards the original, unmodified message to the OM.

**2. Bridge to VHC Transmission:**
   a. **Bridge (Sender):** When the Bridge receives a message from the OM to be forwarded to the VHC (e.g., `SESSION_APPROVED`), it assigns its own `sequence_number`, stores it in a `pending_acks` map for that VHC, and sends it. It then starts a timer.
   b. **VHC (Receiver):** Upon receiving any message from the Bridge, it immediately sends an `ACK` back to the Bridge.
   c. **Bridge (ACK Handling):** Upon receiving the `ACK` from the VHC, the Bridge removes the corresponding entry from its `pending_acks` map.
   d. **Bridge (Timeout/Retry):** If the Bridge's timer expires, it re-sends the message to the VHC.
   e. **Bridge (Inspection):** The Bridge can inspect the message from the OM before forwarding it to trigger internal actions (e.g., configuring the data plane on `SESSION_APPROVED`).

**3. End-to-End Message Flow**
The hop-by-hop mechanism provides a reliable end-to-end communication channel between the VHC and the OM, mediated by the Bridge.

*   **VHC to OM:** When a VHC sends a message, it receives an `ACK` from the Bridge, confirming the message has successfully crossed the unstable link. The VHC then relies on the stable Bridge-OM link for the message to reach the OM. The true end-to-end confirmation is the subsequent response message (e.g., `SESSION_APPROVED` or `SESSION_DENIED`) from the OM. If this response is not received within a higher-level timeout, the VHC can assume a failure in the Bridge-OM link or the OM itself and may retry the entire request.

*   **OM to VHC:** When the OM sends a message, the Bridge guarantees its delivery across the unstable link to the VHC using its own ACK-and-retry mechanism.

  

### 3. Component Implementation Details

#### 3.1. Topic Handler Architecture

To ensure type safety and performance, the gateway will use a collection of specialized, compile-time topic handlers. The `GenericTopicHandler` concept is removed in favor of this more robust approach.

*   **`MessageHandlerBase` (Abstract Base Class):** The existing abstract base class from `message_handler_base.hpp`. It defines the common interface for all topic handlers and provides the crucial link back to the `RosGateway` data plane via a pointer in its constructor. It will be registered with the `RosGateway`.
*   **Specific Handlers:** For each ROS 2 message type that can be offloaded, a specific handler class will be implemented (e.g., `ImageHandler`, `PointCloud2Handler`). Each will inherit from `MessageHandlerBase` and contain the specific `rclcpp::Publisher` and `rclcpp::Subscription` for its type.
*   **`HandlerFactory`:** A factory class will be responsible for creating instances of these specific handlers.
    *   It will maintain a map from a ROS 2 message type string (e.g., `"sensor_msgs/msg/Image"`) to a function that creates the corresponding handler.
    *   The `GatewayController` will use this factory. When an offloading request is approved, the controller will request handlers from the factory based on the types defined in the Task Database.
    *   If the factory does not have a registered handler for a requested type, it will return a `nullptr`. The `GatewayController` will treat this as a critical failure, causing the session to be denied and an error to be logged. This enforces the "fail-fast" requirement.


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
    *   `send_reliable_message()`: A private method that takes a JSON object, assigns a `sequence_number`, adds it to a `pending_acks_` map with a timestamp, and sends it. This method should be used for all outgoing messages to the Bridge.
    *   `send_offload_request()`: Constructs the `OFFLOAD_REQUEST` JSON, including its payload, and calls `send_reliable_message()`.
    *   `send_dp_info()`: Constructs the `DP_INFO` JSON with its payload and calls `send_reliable_message()`.
    *   `client_thread_func()`:
        1.  Connects to the Bridge.
        2.  Immediately calls `send_dp_info()` to transmit the gateway's data plane address.
        3.  Enters a `while(running_)` loop.
        4.  Inside the loop, it calls `transport_->data_available()`.
        5.  When data is available, it reads the string, parses it into a `nlohmann::json` object, and calls a private `handle_received_message(json)` method.
        6.  The loop also periodically checks for timeouts in the `pending_acks_` map and re-sends messages if necessary.
    *   `handle_received_message()`:
        1.  Extracts `message_code` from the JSON.
        2.  Uses a `switch` statement on the `message_code`.
        3.  **Case 200 (`SESSION_APPROVED`):** Parses the payload, invokes the `on_session_approved_` callback, and sends an `ACK`.
        4.  **Case 201 (`SESSION_DENIED`):** Parses the payload, invokes the `on_session_denied_` callback, and sends an `ACK`.
        5.  **Case 202 (`DP_CONNECTION_CONFIRMED`):** Invokes the `on_dp_confirmed_` callback and sends an `ACK`.
        6.  **Case 900 (`ACK`):** Parses the `ack_sequence_number` from the payload and removes the corresponding entry from `pending_acks_`.

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
    *   **ROS 2 Service Callback:** When a request arrives (containing just a `task_id`), it must not block. It should package the `task_id` into a struct and push it onto a thread-safe queue to be processed by the `control_thread_`.
    *   **`control_thread_func()`:** This is the main state machine.
        *   **`State::INITIALIZING`:** Creates the `DiscoveryClient` instance. Transitions to `DISCOVERING`.
        *   **`State::DISCOVERING`:** Creates the `RegistrationRequest` from the ROS 2 parameters. Calls `discovery_client_->start()`, passing `std::bind` versions of its `on_discovery_success` and `on_discovery_failure` methods as callbacks. The thread then waits on a condition variable.
        *   **`State::CONNECTING_TO_BRIDGE`:** This state is entered when `on_discovery_success` is called. It uses the discovered bridge host/port to create and start the `BridgeControlClient`, passing callbacks for `on_dp_confirmed` and others. Transitions to `WAITING_FOR_DP_CONNECTION`.
        *   **`State::WAITING_FOR_DP_CONNECTION`:** The controller waits on a condition variable for the DP handshake to complete.
        *   **`State::OPERATIONAL`:** This state is entered when the `on_dp_confirmed_` callback is invoked (which notifies the condition variable). The controller is now fully active and can accept offloading requests.
    *   **`on_bridge_session_approved()` Callback:** This method is called by the `BridgeControlClient`. It is here that the data plane is finally configured. It will:
        1.  Look up the `request_id` from the approved session's payload to find the corresponding `task_id` in its local session map.
        2.  Look up the `task_id` in the external Task Database to get the list of required input/output topics and their ROS 2 types.
        3.  For each topic, it creates a new handler using the `HandlerFactory`.
        4.  It calls `data_plane_->register_handler()` with the newly created handler.

*   **`RosGateway` Refactoring:**
    1.  **Remove Node Inheritance:** `RosGateway` will no longer inherit from `rclcpp::Node`. It will be a standard C++ class.
    2.  **Constructor Change:** The constructor will accept a `rclcpp::Node::SharedPtr` from the `GatewayController`. This pointer will be stored and passed down to the topic handlers.
    3.  **Remove Parameter Logic:** All ROS 2 parameter handling (`declare_parameter`, `get_parameter`) will be removed from `RosGateway` and moved to `GatewayController`. Configuration values will be passed into `RosGateway`'s methods.
    4.  **Remove Connection Logic:** The `start_receiver` and `stop_receiver` methods will be simplified. They should no longer contain any logic for initiating connections or waiting for them. They will simply start/stop the receiver thread which processes data on an already-established connection.

*   **`TransportBase` and Implementations (`TcpServerTransport`) Refactoring:**
    1.  **Passive Connection:** The `connect()` method in `TcpServerTransport` must be simplified. It should only perform the `socket()`, `bind()`, and `listen()` calls to set up a listening socket. It must **not** call `accept()`.
    2.  **Explicit `accept()`:** A new method, `accept_connection()`, will be responsible for the blocking `accept()` call. The `GatewayController`'s thread will call this method at the appropriate time in its state machine.
    3.  **Remove Internal `accept()` Calls:** The internal calls to `accept_connection()` from within other methods like `data_available()` and `receive_data()` in `TcpServerTransport` **must be removed**. These methods should only operate on an existing connection and not have the side effect of creating a new one.
    4.  **Remove Auto-Reconnection:** Any automatic reconnection or retry logic within the transport classes must be removed. All connection lifecycle decisions will be made by the `GatewayController`.

*   **`MessageHandlerBase` and Handler Refactoring:**
    1.  **Node Pointer Propagation:** The `MessageHandlerBase` constructor will be modified to accept the `rclcpp::Node::SharedPtr` from the `RosGateway`.
    2.  **Handler ROS Operations:** All handlers (e.g., `StringHandler`) will be updated to use this stored node pointer to create their subscriptions and publishers (e.g., `node_ptr_->create_subscription(...)`). They will no longer call these methods on the `gateway_` pointer.

This refactoring ensures a clean separation of concerns: `GatewayController` handles the "when" (state and timing), while `RosGateway` and `TransportBase` handle the "how" (reading and writing bytes).

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
9.  `control_thread_` wakes up, transitions to `CONNECTING_TO_BRIDGE`, creates and starts `BridgeControlClient`. It then transitions to `WAITING_FOR_DP_CONNECTION` and waits.
10. `BridgeControlClient` connects to the Bridge's CP and immediately sends a `DP_INFO` message (code 103) containing the MGW's DP host and port.
11. The Bridge's CP receives the `DP_INFO` message and instructs its DP to connect to the MGW's DP.
12. Once the DP-to-DP connection is established, the Bridge's CP sends a `DP_CONNECTION_CONFIRMED` message (code 202) to the `BridgeControlClient`.
13. `BridgeControlClient` receives the confirmation and invokes the `on_dp_confirmed_` callback in the `GatewayController`.
14. `GatewayController`'s `control_thread_` is notified, wakes up, and transitions to `OPERATIONAL`.
15. The system is now idle and ready to accept offloading requests.

#### 4.2. Scenario: Offloading Request
1.  An external ROS 2 node calls the offload service on the `GatewayController`, providing a `task_id`.
2.  The ROS 2 service callback executes in an `rclcpp` thread. It pushes the `task_id` into a thread-safe queue and returns immediately.
3.  The `GatewayController::control_thread_` (in its `OPERATIONAL` loop) pops the `task_id` from the queue.
4.  The `GatewayController` now orchestrates the new request:
    a. It generates a new, unique `request_id` (e.g., using a `std::atomic<uint64_t>` counter).
    b. It looks up the corresponding `task_name` from the `task_id` using the Task Database.
    c. It creates a local session record mapping the `request_id` to the `task_id` to track the request.
5.  It calls `bridge_control_client_->send_offload_request(...)`, which constructs and sends an `OFFLOAD_REQUEST` (code 100) message containing the generated `request_id`, `task_id`, and `task_name`.
6.  ...Time passes... The Bridge communicates with the OM and gets approval.
7.  The Bridge sends a `SESSION_APPROVED` (code 200) JSON message back. The payload of this message contains the original `request_id`.
8.  `BridgeControlClient`'s thread receives and parses the message, invoking the `on_bridge_session_approved_` callback in the `GatewayController`.
9.  `GatewayController::on_bridge_session_approved_` uses the `request_id` from the payload to look up the task's details in its local session map and in the Task Database.
10. It loops through the topics for the approved task, using the `HandlerFactory` to create and register a specific handler (which inherits from `MessageHandlerBase`) for each one via `data_plane_->register_handler()`.
11. The data plane is now live for that task. Data begins to flow.



### APENDIX A - control plane protocol library
``` cpp
#ifndef CONTROL_PLANE_PROTOCOL_HPP
#define CONTROL_PLANE_PROTOCOL_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace gateway::protocol {

// Use nlohmann::json for convenience
using json = nlohmann::json;

/**
 * @brief Defines the types of messages in the Control Plane Protocol.
 */
enum class ControlPlaneMessageType {
    UNKNOWN,
    REQUEST_OFFLOADING,
    REQUEST_RESPONSE,
    SESSION_KEEPALIVE,
    SESSION_TEARDOWN
};

/**
 * @brief Represents a request from the Gateway to the Bridge to offload a task.
 */
struct OffloadingRequest {
    std::string request_id;
    std::string component_id; // e.g., "10:1"
    std::string task_id;
};

/**
 * @brief Represents the Bridge's response to an offloading request.
 */
struct OffloadingResponse {
    std::string request_id;
    bool approved;
    std::string message;
    std::string data_plane_host; // The host for the data connection
    int data_plane_port;         // The port for the data connection
};

/**
 * @brief Represents a keepalive message to maintain an active session.
 */
struct SessionKeepalive {
    std::string request_id;
    std::string component_id;
};

// --- Serialization Functions ---

/**
 * @brief Serializes an OffloadingRequest to a JSON string.
 */
inline std::string serialize(const OffloadingRequest& req) {
    json j = {
        {"type", "REQUEST_OFFLOADING"},
        {"request_id", req.request_id},
        {"component_id", req.component_id},
        {"task_id", req.task_id}
    };
    return j.dump();
}

/**
 * @brief Serializes a SessionKeepalive to a JSON string.
 */
inline std::string serialize(const SessionKeepalive& req) {
    json j = {
        {"type", "SESSION_KEEPALIVE"},
        {"request_id", req.request_id},
        {"component_id", req.component_id}
    };
    return j.dump();
}


// --- Deserialization Functions ---

/**
 * @brief Deserializes a JSON string into an OffloadingResponse.
 * @throws std::invalid_argument if parsing or validation fails.
 */
inline OffloadingResponse deserialize_response(const std::string& json_str) {
    OffloadingResponse resp;
    try {
        json j = json::parse(json_str);
        if (j.at("type") != "REQUEST_RESPONSE") {
            throw std::invalid_argument("Invalid message type for response.");
        }
        j.at("request_id").get_to(resp.request_id);
        j.at("approved").get_to(resp.approved);
        j.at("message").get_to(resp.message);
        if (resp.approved) {
            j.at("data_plane_host").get_to(resp.data_plane_host);
            j.at("data_plane_port").get_to(resp.data_plane_port);
        }
    } catch (const json::exception& e) {
        throw std::invalid_argument("Failed to parse OffloadingResponse: " + std::string(e.what()));
    }
    return resp;
}

/**
 * @brief Gets the message type from a raw JSON string.
 */
inline ControlPlaneMessageType get_message_type(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        std::string type_str = j.value("type", "UNKNOWN");
        if (type_str == "REQUEST_RESPONSE") return ControlPlaneMessageType::REQUEST_RESPONSE;
        if (type_str == "SESSION_TEARDOWN") return ControlPlaneMessageType::SESSION_TEARDOWN;
    } catch (const json::exception&) {
        return ControlPlaneMessageType::UNKNOWN;
    }
    return ControlPlaneMessageType::UNKNOWN;
}

} // namespace gateway::protocol

#endif // CONTROL_PLANE_PROTOCOL_HPP


```
### APENDIX B Discovery protocol library 
``` cpp
#ifndef DISCOVERY_PROTOCOL_HPP
#define DISCOVERY_PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <variant>

namespace discovery_protocol {

// Error codes for protocol operations
enum class ProtocolStatus {
    OK = 0,
    BUFFER_TOO_SMALL,
    INVALID_MESSAGE_TYPE,
    MALFORMED_MESSAGE,
    MISSING_REQUIRED_FIELDS,
    INVALID_FORMAT,
    FIELD_TOO_LONG,
    INCOMPLETE_MESSAGE
};

enum class ComponentType {
    VEHICLE,
    MEC,
    BRIDGE,
    OFFLOAD_MANAGER,
    TEST
};

enum class IdRequestType {
    AUTOMATIC,
    STATIC
};

enum class ResponseCode {
    SUCCESS,
    WAIT,
    ID_CONFLICT,
    INVALID_REQUEST,
    GENERAL_ERROR
};

enum class MessageType {
    REGISTRATION_REQUEST,
    REGISTRATION_RESPONSE,
    ERROR,
    KEEPALIVE_PING,
    KEEPALIVE_RESPONSE
};

enum class ErrorCode {
    CONFIGURATION,
    NETWORK,
    INTERNAL,
    PROTOCOL
};

struct RegistrationRequest {
    ComponentType componentType;
    IdRequestType idRequestType;
    uint8_t groupId;
    uint8_t idInGroup;
    std::string componentName;
    std::string listenAddress;
    std::string listenPort;
    std::string humanReadableMessage;
};

struct RegistrationResponse {
    ResponseCode responseCode;
    uint8_t assignedGroupId;
    uint8_t assignedIdInGroup;
    std::string connectionTargetType;
    std::string connectionTargetAddress;
    std::string connectionTargetPort;
    uint16_t connectionTargetId;
    std::string configJson;
    std::string humanReadableMessage;
};

struct KeepalivePing {
    std::string componentId;
    std::string status;
    std::string humanReadableMessage;
};

struct KeepaliveResponse {
    std::string response;
    int nextIntervalMs;
    std::string humanReadableMessage;
};

struct ErrorMessage {
    ErrorCode errorCode;
    std::string humanReadableMessage;
};

using MessageVariant = std::variant<
    RegistrationRequest,
    RegistrationResponse,
    ErrorMessage,
    KeepalivePing,
    KeepaliveResponse
>;

struct Message {
    MessageType type;
    MessageVariant data;

    Message() : type(MessageType::ERROR), data(ErrorMessage{}) {}
    explicit Message(const RegistrationRequest& req) : type(MessageType::REGISTRATION_REQUEST), data(req) {}
    explicit Message(const RegistrationResponse& res) : type(MessageType::REGISTRATION_RESPONSE), data(res) {}
    explicit Message(const KeepalivePing& ping) : type(MessageType::KEEPALIVE_PING), data(ping) {}
    explicit Message(const KeepaliveResponse& pong) : type(MessageType::KEEPALIVE_RESPONSE), data(pong) {}
    explicit Message(const ErrorMessage& err) : type(MessageType::ERROR), data(err) {}
};

ProtocolStatus encode_message(const Message& message, std::string& output);
ProtocolStatus decode_message(const std::string& input, Message& output);

ProtocolStatus encode_registration_request(const RegistrationRequest& request, std::string& output);
ProtocolStatus decode_registration_request(const std::string& input, RegistrationRequest& output);

ProtocolStatus encode_registration_response(const RegistrationResponse& response, std::string& output);
ProtocolStatus decode_registration_response(const std::string& input, RegistrationResponse& output);

ProtocolStatus encode_keepalive_ping(const KeepalivePing& ping, std::string& output);
ProtocolStatus decode_keepalive_ping(const std::string& input, KeepalivePing& output);

ProtocolStatus encode_keepalive_response(const KeepaliveResponse& response, std::string& output);
ProtocolStatus decode_keepalive_response(const std::string& input, KeepaliveResponse& output);

ProtocolStatus encode_error_message(const ErrorMessage& error, std::string& output);
ProtocolStatus decode_error_message(const std::string& input, ErrorMessage& output);

const char* protocol_status_to_string(ProtocolStatus status);
std::string component_type_to_string(ComponentType type);
ComponentType string_to_component_type(const std::string& str);
std::string response_code_to_string(ResponseCode code);
ResponseCode string_to_response_code(const std::string& str);
std::string error_code_to_string(ErrorCode code);
ErrorCode string_to_error_code(const std::string& str);

std::vector<std::string> split_string(const std::string& str, char delimiter);

} // namespace discovery_protocol

#endif // DISCOVERY_PROTOCOL_HPP

```
``` cpp
#include "discovery_protocol/protocol.hpp"
#include <sstream>
#include <cstring>
#include <vector>
#include <algorithm>

namespace discovery_protocol {

// Helper function to split strings, useful for parsing
std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// --- Enum to String Conversion ---

const char* protocol_status_to_string(ProtocolStatus status) {
    switch (status) {
        case ProtocolStatus::OK: return "OK";
        case ProtocolStatus::BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case ProtocolStatus::INVALID_MESSAGE_TYPE: return "INVALID_MESSAGE_TYPE";
        case ProtocolStatus::MALFORMED_MESSAGE: return "MALFORMED_MESSAGE";
        case ProtocolStatus::MISSING_REQUIRED_FIELDS: return "MISSING_REQUIRED_FIELDS";
        case ProtocolStatus::INVALID_FORMAT: return "INVALID_FORMAT";
        case ProtocolStatus::FIELD_TOO_LONG: return "FIELD_TOO_LONG";
        case ProtocolStatus::INCOMPLETE_MESSAGE: return "INCOMPLETE_MESSAGE";
        default: return "UNKNOWN_STATUS";
    }
}

std::string component_type_to_string(ComponentType type) {
    switch (type) {
        case ComponentType::VEHICLE: return "V";
        case ComponentType::MEC: return "M";
        case ComponentType::BRIDGE: return "B";
        case ComponentType::OFFLOAD_MANAGER: return "O";
        case ComponentType::TEST: return "T";
        default: return "?";
    }
}

ComponentType string_to_component_type(const std::string& str) {
    if (str == "V") return ComponentType::VEHICLE;
    if (str == "M") return ComponentType::MEC;
    if (str == "B") return ComponentType::BRIDGE;
    if (str == "O") return ComponentType::OFFLOAD_MANAGER;
    if (str == "T") return ComponentType::TEST;
    return ComponentType::TEST; // Default/unknown
}

// --- Message-level encode/decode ---

ProtocolStatus encode_message(const Message& message, std::string& output) {
    switch (message.type) {
        case MessageType::REGISTRATION_REQUEST:
            return encode_registration_request(std::get<RegistrationRequest>(message.data), output);
        case MessageType::REGISTRATION_RESPONSE:
            return encode_registration_response(std::get<RegistrationResponse>(message.data), output);
        case MessageType::KEEPALIVE_PING:
            return encode_keepalive_ping(std::get<KeepalivePing>(message.data), output);
        case MessageType::KEEPALIVE_RESPONSE:
            return encode_keepalive_response(std::get<KeepaliveResponse>(message.data), output);
        case MessageType::ERROR:
            return encode_error_message(std::get<ErrorMessage>(message.data), output);
        default:
            return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
}

ProtocolStatus decode_message(const std::string& input, Message& output) {
    if (input.size() < 8 || input.substr(0, 5) != "DISC:") {
        return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
    std::string msg_type = input.substr(5, 3);
    if (msg_type == "REG") {
        RegistrationRequest req;
        ProtocolStatus status = decode_registration_request(input, req);
        if (status == ProtocolStatus::OK) output = Message(req);
        return status;
    } else if (msg_type == "ACK") {
        RegistrationResponse res;
        ProtocolStatus status = decode_registration_response(input, res);
        if (status == ProtocolStatus::OK) output = Message(res);
        return status;
    } else if (msg_type == "PNG") {
        KeepalivePing ping;
        ProtocolStatus status = decode_keepalive_ping(input, ping);
        if (status == ProtocolStatus::OK) output = Message(ping);
        return status;
    } else if (msg_type == "PON") {
        KeepaliveResponse pong;
        ProtocolStatus status = decode_keepalive_response(input, pong);
        if (status == ProtocolStatus::OK) output = Message(pong);
        return status;
    } else if (msg_type == "ERR") {
        ErrorMessage err;
        ProtocolStatus status = decode_error_message(input, err);
        if (status == ProtocolStatus::OK) output = Message(err);
        return status;
    }
    return ProtocolStatus::INVALID_MESSAGE_TYPE;
}

// --- Individual Message Type Implementations ---

ProtocolStatus encode_registration_request(const RegistrationRequest& request, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:REG;"
       << component_type_to_string(request.componentType) << ";"
       << (request.idRequestType == IdRequestType::AUTOMATIC ? "A" : "S") << ";"
       << static_cast<int>(request.groupId) << ";"
       << static_cast<int>(request.idInGroup) << ";"
       << request.componentName << ";"
       << request.listenAddress << ";"
       << request.listenPort << ";"
       << request.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_registration_request(const std::string& input, RegistrationRequest& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 9) return ProtocolStatus::MALFORMED_MESSAGE;
    try {
        output.componentType = string_to_component_type(parts[1]);
        output.idRequestType = (parts[2] == "A") ? IdRequestType::AUTOMATIC : IdRequestType::STATIC;
        output.groupId = static_cast<uint8_t>(std::stoi(parts[3]));
        output.idInGroup = static_cast<uint8_t>(std::stoi(parts[4]));
        output.componentName = parts[5];
        output.listenAddress = parts[6];
        output.listenPort = parts[7];
        output.humanReadableMessage = parts[8];
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    return ProtocolStatus::OK;
}

ProtocolStatus encode_registration_response(const RegistrationResponse& response, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:ACK;"
       << static_cast<int>(response.responseCode) << ";"
       << static_cast<int>(response.assignedGroupId) << ";"
       << static_cast<int>(response.assignedIdInGroup) << ";"
       << response.connectionTargetType << ";"
       << response.connectionTargetAddress << ";"
       << response.connectionTargetPort << ";"
       << response.connectionTargetId << ";"
       << response.configJson << ";"
       << response.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_registration_response(const std::string& input, RegistrationResponse& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 10) return ProtocolStatus::MALFORMED_MESSAGE;
    try {
        output.responseCode = static_cast<ResponseCode>(std::stoi(parts[1]));
        output.assignedGroupId = static_cast<uint8_t>(std::stoi(parts[2]));
        output.assignedIdInGroup = static_cast<uint8_t>(std::stoi(parts[3]));
        output.connectionTargetType = parts[4];
        output.connectionTargetAddress = parts[5];
        output.connectionTargetPort = parts[6];
        output.connectionTargetId = static_cast<uint16_t>(std::stoi(parts[7]));
        output.configJson = parts[8];
        output.humanReadableMessage = parts[9];
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    return ProtocolStatus::OK;
}

ProtocolStatus encode_keepalive_ping(const KeepalivePing& ping, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:PNG;" << ping.componentId << ";" << ping.status << ";" << ping.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_keepalive_ping(const std::string& input, KeepalivePing& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 4) return ProtocolStatus::MALFORMED_MESSAGE;
    output.componentId = parts[1];
    output.status = parts[2];
    output.humanReadableMessage = parts[3];
    return ProtocolStatus::OK;
}

ProtocolStatus encode_keepalive_response(const KeepaliveResponse& response, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:PON;" << response.response << ";" << response.nextIntervalMs << ";" << response.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_keepalive_response(const std::string& input, KeepaliveResponse& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 4) return ProtocolStatus::MALFORMED_MESSAGE;
    try {
        output.response = parts[1];
        output.nextIntervalMs = std::stoi(parts[2]);
        output.humanReadableMessage = parts[3];
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    return ProtocolStatus::OK;
}

ProtocolStatus encode_error_message(const ErrorMessage& error, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:ERR;" << static_cast<int>(error.errorCode) << ";" << error.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_error_message(const std::string& input, ErrorMessage& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 3) return ProtocolStatus::MALFORMED_MESSAGE;
    try {
        output.errorCode = static_cast<ErrorCode>(std::stoi(parts[1]));
        output.humanReadableMessage = parts[2];
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    return ProtocolStatus::OK;
}

} // namespace discovery_protocol

```