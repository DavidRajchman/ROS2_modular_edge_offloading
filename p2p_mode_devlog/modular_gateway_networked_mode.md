# Modular Gateway Architecture (Networked Mode)

The following diagram illustrates the logical architecture and data flow of a single Modular Gateway running in the standard **Networked Mode** (connected to the central Bridge and Offloading Manager).

## Logical Architecture Diagram

```text
                             +-----------------------------------------------------+
                             |              CENTRAL ORCHESTRATION                  |
                             |  [Offloading Manager] <------> [Bridge Server]      |
                             +-----------------------------------------------------+
                                                                ^          ^
                                    (Control Plane TCP)         |          |  (Data Plane TCP)
                                                                |          |
+---------------------------------------------------------------|----------|------------------+
| MODULAR GATEWAY NODE                                          |          |                  |
|                                                               v          |                  |
|  +--------------------------+                         +------------------|-+                |
|  |     BridgeCPClient       | <--- (CP Messages) ---> |   TCP Sender /   |                |
|  |  (Control Plane Logic)   |                         |   TCP Receiver   |                |
|  +--------------------------+                         |   (Data Plane)   |                |
|               |                                       +--------------------+                |
|               | (e.g. SESSION_APPROVED)                          ^                          |
|               v                                                  |                          |
|  +--------------------------+                                    | (Framed serialized data) |
|  |    GatewayController     |                                    |                          |
|  |  (Lifecycle & Routing)   |                                    |                          |
|  +--------------------------+                          +--------------------+               |
|               |                                        | Thread-Safe Queue  |               |
|               | (Instantiate handler for topic)        |  (Message Buffer)  |               |
|               v                                        +--------------------+               |
|  +--------------------------+                                    ^                          |
|  |     HandlerFactory       |                                    |                          |
|  | (Dynamic Instantiation)  |                                    |                          |
|  +--------------------------+                                    |                          |
|               |                                                  |                          |
|               | (Creates based on MessageType enum)              |                          |
|               |                                                  |                          |
|   +-----------+---------------+---------------+                  |                          |
|   v                           v               v                  |                          |
| +-----------------+   +---------------+   +-----------------+    |                          |
| |  StringHandler  |   |LaserScanHndlr |   |JointStateHandler|    |                          |
| |(MessageHandler) |   |(MessageHandler|   |(MessageHandler) |    |                          |
| +-----------------+   +---------------+   +-----------------+    |                          |
|         ^                     ^                   ^              |                          |
|         | (ROS2 Callback)     |                   |              |                          |
+---------|---------------------|-------------------|--------------|--------------------------+
          v                     v                   v              |
    [Topic: /chatter]     [Topic: /scan]  [Topic: /joint_states]   |
                                                                   |
          >--------------------------------------------------------+
             (Handlers serialize incoming ROS2 data via cJSON 
              and push to the Thread-Safe Queue)
```

## Data Plane Flow (Topic -> TCP)

1. **ROS2 Subscription:** A dynamically instantiated `MessageHandler` (e.g., `JointStateHandler`) receives a standard ROS2 message via its callback.
2. **Serialization:** The handler immediately serializes the ROS2 payload into a uniform format (`cJSON`) using statically allocated buffers (the "Batcher Task" methodology).
3. **Queueing:** The serialized buffer is pushed into the `Thread-Safe Queue` shared across the gateway.
4. **Transmission:** The `TCP Sender` thread continuously polls the queue, wraps the JSON payload in a TCP frame (adding length headers), and transmits it over the Data Plane socket directly to the Bridge.

## Control Plane Flow & Modularity

1. **CP Registration:** The `BridgeCPClient` establishes a dedicated TCP connection to the central Bridge.
2. **Task Assignment:** When the Offloading Manager (OM) decides to offload a topic, it sends a `SESSION_APPROVED` message down to the `BridgeCPClient`.
3. **Factory Instantiation:** The `BridgeCPClient` triggers the `GatewayController`, which reads the target topic and `MessageType`. It calls the `HandlerFactory`.
4. **Dynamic Handlers:** The `HandlerFactory` maps the requested type (e.g., `MessageType::smJOINTSTATE`) to a specific constructor, dynamically spawning a new `JointStateHandler` on the fly. 
5. This modular design means the core Gateway code never needs to be recompiled just to add a new topic—developers simply write a new `MessageHandlerBase` derived class and register it in the `HandlerFactory`.

## Detailed Data Plane Flow Diagram

This second diagram expands on the data plane, explicitly demonstrating the **bidirectional** flow of data (Incoming vs Outgoing) through the handlers.

```text
                             +-----------------------------------------------------+
                             |              CENTRAL ORCHESTRATION                  |
                             |  [Offloading Manager] <------> [Bridge Server]      |
                             +-----------------------------------------------------+
                                                                ^          ^
                                    (Control Plane TCP)         |          |  (Data Plane TCP)
                                                                |          |
+---------------------------------------------------------------|----------|------------------+
| MODULAR GATEWAY NODE                                          |          |                  |
|                                                               v          |                  |
|  +--------------------------+                         +-------|----------|---------+        |
|  |     BridgeCPClient       | <--- (CP Messages) ---> |  TCP Receiver / Sender     |        |
|  |  (Control Plane Logic)   |                         |   (Data Plane Node)        |        |
|  +--------------------------+                         +-------|----------|---------+        |
|               |                                         |                   ^               |
|               | (SESSION_APPROVED)                      | (Incoming JSON)   | (Outgoing Q.) |
|               v                                         v                   |               |
|  +--------------------------+                           |       +-----------|-------+       |
|  |    GatewayController     | <-------------------------+       | Thread-Safe Queue |       |
|  |  (Lifecycle & Routing)   |                                   |    (Outgoing)     |       |
|  +--------------------------+     (TCP Receiver                 +-------------------+       |
|               |                    routes message                    ^          ^           |
|               | (Instantiates)     to handler via                    |          |           |
|               v                    MessageType)                      |          |           |
|  +--------------------------+          |                             |          |           |
|  |     HandlerFactory       |          |                             |          |           |
|  +--------------------------+          |                             |          |           |
|                                        v                 |                  |               |
|  +-------------------------------------|-----------------|------------------|------------+  |
|  |                     MESSAGE HANDLER INSTANCE (e.g., JointStateHandler)   |            |  |
|  |                                     |                 |                  |            |  |
|  |  +-------------------------+        |      +----------|------------------|-------+    |  |
|  |  |      INCOMING PATH      | <------+      |      OUTGOING PATH                |    |  |
|  |  | (TCP -> Gateway -> ROS2)|               | (ROS2 -> Gateway -> TCP)          |    |  |
|  |  |                         |               |                                   |    |  |
|  |  | 1. Receives framed JSON |               | 1. ROS2 Subscription Callback     |    |  |
|  |  | 2. Deserializes (cJSON) |               | 2. Serializes (cJSON)             |    |  |
|  |  | 3. Converts to ROS msg  |               | 3. Writes to static buffer        |    |  |
|  |  | 4. ROS2 Publisher       |               | 4. Pushes to Thread-Safe Queue    |    |  |
|  |  +-------------------------+               +-----------------------------------+    |  |
|  +-----------------------------------------------------------------------------------+  |
|                 |                                              ^                        |
|                 v (ROS2 publish)                               | (ROS2 subscribe)       |
+-----------------|----------------------------------------------|------------------------+
                  |                                              |
        [ROS2 Topic: /joint_states]                    [ROS2 Topic: /joint_states]
```

## Diagram Arrow Breakdown (Code Mapping)

Below is an exhaustive list mapping every arrow in the diagram to its exact software equivalent in the source code:

1.  **`[Offloading Manager] <------> [Bridge Server]`**
    *   **Start/End:** Bidirectional TCP connection between the Python OM and the C++ Bridge Server.
    *   **Code Flow:** External backend components not present in the Gateway source code.
2.  **`[Bridge Server] <--- (Control Plane TCP) ---> [BridgeCPClient]`**
    *   **Start/End:** Control Plane network socket (Vertical arrow).
    *   **Code Flow:** The `BridgeCPClient` manages a dedicated TCP socket specifically for receiving JSON lifecycle commands from the central Bridge.
3.  **`[Bridge Server] <--- (Data Plane TCP) --- [TCP Receiver / Sender]`**
    *   **Start/End:** Data Plane network socket (Vertical arrow).
    *   **Code Flow:** The `RosGateway` manages a separate, high-throughput raw TCP socket solely for passing binary framed payloads to/from the Bridge.
4.  **`[BridgeCPClient] <--- (CP Messages) ---> [TCP Receiver / Sender]`**
    *   **Start/End:** Diagram visual boundary (Horizontal arrow).
    *   **Code Flow:** Structurally, there is no direct runtime code flow here in the C++ execution. This arrow is a purely illustrative visual divider separating the Control Plane logic block from the Data Plane logic block in the drawing.
5.  **`[BridgeCPClient] ---> (SESSION_APPROVED) ---> [GatewayController]`**
    *   **Start/End:** Internal callback (Vertical arrow).
    *   **Code Flow:** When `BridgeCPClient` parses a `SESSION_APPROVED` JSON command, it triggers a registered callback inside `GatewayController` instructing it to spin up a new topic handler.
6.  **`[GatewayController] ---> (Instantiates) ---> [HandlerFactory]`**
    *   **Start/End:** Synchronous function call (Vertical arrow).
    *   **Code Flow:** `GatewayController` extracts the `MessageType` ID from the session parameters and calls `HandlerFactory::get_or_create(MessageType)`.
7.  **`[TCP Receiver] ---> (Incoming JSON) ---> [RosGateway Router]`**
    *   **Start/End:** Transport receiver thread to `RosGateway` message parser (Right-to-Left routing arrow).
    *   **Code Flow:** The `RosGateway::receiver_thread_func()` receives data from the `TcpClientTransport` (or Server), decodes the magic headers, and conceptually triggers its internal routing logic. *(Note: The diagram visually places the router near the GatewayController, but the Controller itself does not touch data).*
8.  **`[RosGateway Router] ---> (routes message...) ---> [INCOMING PATH]`**
    *   **Start/End:** `RosGateway` router loop to Message Handler instance (Vertical dropping arrow to the left).
    *   **Code Flow:** Based on the decoded `MessageType` header, the `RosGateway` iterates through its internal list of active handlers and directly invokes `handler->process_and_publish_received_msg()` on the matching `MessageHandler` instance.
9.  **`[INCOMING PATH] ---> (ROS2 publish) ---> [ROS2 Topic]`**
    *   **Start/End:** MessageHandler to ROS2 DDS (Vertical arrow).
    *   **Code Flow:** Inside the handler (e.g., `JointStateHandler`), the `cJSON` payload is unpacked into a native `sensor_msgs::msg::JointState` struct and fired into the local ROS2 network via `publisher_->publish()`.
10. **`[ROS2 Topic] ---> (ROS2 subscribe) ---> [OUTGOING PATH]`**
    *   **Start/End:** ROS2 DDS to MessageHandler callback (Vertical arrow).
    *   **Code Flow:** The native ROS2 execution thread triggers the handler's active subscription callback (e.g., `JointStateHandler::ros_topic_callback()`) whenever a local node publishes to the topic.
11. **`[OUTGOING PATH] ---> (Pushes serialized payload) ---> [Thread-Safe Queue]`**
    *   **Start/End:** ROS2 callback thread to the concurrent `std::queue` / `Batcher` (Vertical arrow).
    *   **Code Flow:** The handler immediately serializes the ROS2 message into a pre-allocated static buffer array (using `cJSON`), and pushes that framed buffer pointer into `RosGateway`'s shared thread-safe outgoing queue.
12. **`[Thread-Safe Queue] ---> (Outgoing Q.) ---> [TCP Sender]`**
    *   **Start/End:** Queue to `RosGateway` background sender thread (Vertical arrow).
    *   **Code Flow:** `RosGateway::sender_thread_func()` continuously polls the queue. When data is available, it pops the buffer, appends the binary magic headers (MessageType, lengths), and pushes it out over the raw `transport_->send()` data plane TCP socket.
