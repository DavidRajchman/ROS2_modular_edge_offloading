## Modular Gateway External Behavior Reference (v0.13.0) 

*AI generated and human reviewed*

### 1. Overview

The Modular Gateway (Modular GW) is a ROS 2 system designed to bridge ROS 2 topics between different environments over a network connection. It allows for selective forwarding of messages, with configurable behavior for how messages are handled on both the ROS 2 side and the network side.

It consists of gateway nodes (`gateway_client` and `gateway_server`) that manage network connections and message handlers that process specific ROS 2 message types.

### 2. Core Concepts

*   **Gateway Nodes**: These are the main executables. currently designed for testing. ROS2 and TCP behaviours are completely independant (tcp server can be a ros2 listener or sender)
    *   `gateway_client`: Initiates a connection to a `gateway_server`.
    *   `gateway_server`: Listens for and accepts connections from `gateway_client` nodes.
*   **Message Handlers (Conceptual)**: Internally, the gateway uses handlers for specific ROS 2 message types (e.g., `std_msgs/msg/String`, `sensor_msgs/msg/LaserScan`). From an external perspective:
    *   When a handler is configured in **subscriber mode**, it listens to a specific ROS 2 topic. Messages received on this topic are packaged according to the Modular GW network protocol and sent over the active network connection.
    *   When a handler is configured in **publisher mode**, it listens for messages of its designated type arriving from the network. These network messages are then unpackaged and published onto a specific ROS 2 topic.
*   **Transport Layer**: The gateway uses a transport layer for network communication. Currently, TCP is the primary supported transport.
    *   **Client Mode**: The gateway attempts to establish a TCP connection to a specified server host and port.
    *   **Server Mode**: The gateway listens for incoming TCP connections on a specified port. It typically handles one client connection at a time.

### 3. Network Protocol

All messages exchanged over the network by the Modular GW adhere to a specific binary header format, followed by a payload.

#### 3.1. Message Header Structure

The header is a minimum of 11 bytes, plus the length of the topic name. All multi-byte integer fields are transmitted in **big-endian** order.

| Offset (bytes) | Length (bytes) | Field                 | Description                                                                                                |
| :------------- | :------------- | :-------------------- | :--------------------------------------------------------------------------------------------------------- |
| 0              | 2              | Magic Number          | Always `0xA5C3`. Identifies the start of a Modular GW message.                                             |
| 2              | 1              | Flags                 | A bitmask indicating various message properties and protocol version. See section 3.1.1.                   |
| 3              | 1              | Message Type          | An 8-bit unsigned integer identifying the type of the payload. See section 3.2.                            |
| 4              | 1              | ID Group              | An 8-bit identifier for the group of the sender (e.g., VHC DNS-assigned, VHC manually-assigned, MEC, etc.). |
| 5              | 1              | Identifier in Group   | An 8-bit unique identifier of the sender within its `ID Group`.                                            |
| 6              | 4              | Payload Size          | A 32-bit unsigned integer representing the size of the message payload (following the topic name) in bytes.  |
| 10             | 1              | Topic Length          | An 8-bit unsigned integer representing the length of the `Topic Name` string in bytes (max 255).           |
| 11             | `Topic Length` | Topic Name            | The ROS 2 topic name associated with the message, UTF-8 encoded.                                           |
| 11 + `Topic Length` | `Payload Size` | Payload             | The actual message data. Its interpretation depends on the `Message Type`.                                 |

**Note for Routing Intermediaries (e.g., a Bridge):** While the `Topic Name` is essential for the end-point Modular GWs to interact with their respective ROS 2 environments, an intermediary system designed for high-speed routing might primarily use a combination of `ID Group`, `Identifier in Group` (as a composite source ID), and the `Message Type` byte as its primary key for routing decisions. This assumes a system-level convention or external configuration (e.g., from an Orchestrator) that maps these key components to specific data flows, especially when a unique `(Source ID, Message Type)` pair consistently corresponds to a single logical data stream (e.g., a specific ROS 2 topic) for routing purposes. The full `Topic Name` would still need to be parsed by such an intermediary to correctly determine the total message length and forward the message intact.

##### 3.1.1. Flags Byte

The Flags byte is an 8-bit field where each bit (or group of bits) has a specific meaning:

*   **Bit 7 (MSB): Timestamp (`0x80`)**: If set, the message payload is expected to contain or be prefixed by timestamp information relevant to the message type. The exact format of this timestamp is dependent on the `Message Type` and how the corresponding handler processes it.
*   **Bit 6: Fragmented (`0x40`)**: If set, the message is part of a larger message that has been fragmented. (Note: Full fragmentation logic and reassembly might have specific handler dependencies not detailed here).
*   **Bit 5: Serialized (`0x20`)**: If set, the message payload contains data that has been serialized using ROS 2 serialization mechanisms (e.g., CDR). If not set, the payload might be a more raw or custom format. This is crucial for handlers to correctly interpret/reconstruct ROS 2 messages.
*   **Bit 4: Checksum (`0x10`)**: If set, the message includes a checksum for validation. (Note: The checksum calculation method and its location relative to the payload are not defined at this general protocol level and would be specific to implementations using this flag).
*   **Bit 3: Compressed (`0x08`)**: If set, the message payload is compressed. (Note: The compression algorithm is not defined at this general protocol level).
*   **Bits 0-2: Version (`0x07`)**: A 3-bit field representing the protocol version (values 0-7). Currently, version `0` is standard.

#### 3.2. Message Payload

The content and structure of the `Payload` field are determined by the `Message Type` field in the header and the `Serialized` flag. If the `Serialized` flag is set, the payload is typically the CDR-serialized form of a ROS 2 message.

#### 3.3. Supported Message Types (`MessageType`)

The `Message Type` field in the header is a `uint8_t` corresponding to the following known types:

| Value | Name        | Description                     | Typical ROS 2 Type (if applicable) |
| :---- | :---------- | :------------------------------ | :--------------------------------- |
| 1     | `STRING`    | Text/string data                | `std_msgs::msg::String`            |
| 2     | `INT32`     | 32-bit signed integer           | `std_msgs::msg::Int32`             |
| 3     | `FLOAT32`   | 32-bit floating point number    | `std_msgs::msg::Float32`           |
| 4     | `BOOL`      | Boolean value                   | `std_msgs::msg::Bool`              |
| 11    | `smLASERSCAN`| LaserScan sensor data           | `sensor_msgs::msg::LaserScan`      |

### 4. Gateway Node Configuration

The Modular GW provides two main executables: `gateway_client` and `gateway_server`. Their behavior is configured through ROS 2 parameters.

#### 4.1. Common Parameters

These parameters are generally applicable to both `gateway_client` and `gateway_server` nodes:

*   `transport_type` (string, default: `"tcp"`): Specifies the network transport to use. Currently, only "tcp" is fully supported.
*   `server_port` (int, default: `12888`):
    *   For `gateway_server`: The TCP port on which the server will listen for incoming connections.
    *   For `gateway_client`: The TCP port of the remote server to connect to.
*   `auto_start_receiver` (bool, default: `true`): If true, the gateway node automatically attempts to start its message receiving mechanism (and establish/listen for connections) upon startup.
*   `wait_for_connection` (bool, default: `true`):
    *   For `gateway_client`: If `auto_start_receiver` is true, this controls whether the client waits for a successful connection before proceeding with other initializations or if it attempts connection in the background.
    *   For `gateway_server`: If `auto_start_receiver` is true, this typically means the server starts listening immediately.
*   `connection_timeout_ms` (int, default: `5000`):
    *   For `gateway_client`: When `wait_for_connection` is true, this is the maximum time (in milliseconds) the client will attempt to connect before failing or giving up on the initial synchronous connection attempt.
*   `receiver_sleep_time_us` (int, default: `1000`): The time (in microseconds) the receiver thread sleeps when no incoming data is immediately available, affecting CPU usage versus network responsiveness.
*   `id_group` (int, default: `0`): The `ID Group` byte that this gateway instance will use in the headers of messages it sends.
*   `identifier_in_group` (int, default: `0`): The `Identifier in Group` byte that this gateway instance will use in the headers of messages it sends.

#### 4.2. Client-Specific Parameters (`gateway_client`)

*   `server_host` (string, default: `"127.0.0.1"`): The hostname or IP address of the `gateway_server` to connect to.
*   `max_retries` (int, default: `3`): The maximum number of connection attempts the client will make if the initial connection fails (behavior might be tied to `wait_for_connection` logic).

#### 4.3. Server-Specific Parameters (`gateway_server`)

*   (Currently, `server_port` is the primary distinct parameter for server listening configuration. `max_connections` is an internal parameter for `TcpServerTransport` defaulting to 1, meaning it handles one client at a time).

### 5. Message Handler Configuration and Behavior

Message handlers are configured within the gateway application (e.g., in gateway_client_main.cpp or gateway_server_main.cpp). Their configuration dictates how ROS 2 topics are bridged to the network. The primary external configuration for a handler is its `HandlerMode`.

*   **`HandlerMode`**:
    *   `SUBSCRIBER_ONLY`:
        *   **ROS 2 Behavior**: The handler subscribes to a pre-defined ROS 2 topic (specific to the handler type, e.g., `/scan` for a LaserScan handler).
        *   **Network Behavior**: When a message is received on its ROS 2 topic, the handler packages it into the Modular GW network protocol (header + payload) and sends it over the active network connection. The `Topic Name` in the network header will be the ROS 2 topic name it subscribed to.
    *   `PUBLISHER_ONLY`:
        *   **ROS 2 Behavior**: The handler creates a ROS 2 publisher for a specific topic.
        *   **Network Behavior**: When a Modular GW network message of a compatible `Message Type` is received, and its `Topic Name` matches what the handler is configured to publish, the handler unpacks the payload and publishes it as a ROS 2 message on its designated topic.
    *   `BOTH`:
        *   The handler performs both `SUBSCRIBER_ONLY` and `PUBLISHER_ONLY` actions. It will subscribe to a ROS 2 topic and send outgoing messages, and it will also listen for incoming network messages to publish to a (potentially different) ROS 2 topic.

*   **Handler-Specific Parameters**: Individual handlers (e.g., for `String`, `LaserScan`) will have their own ROS 2 parameters to configure aspects like the specific ROS 2 topic name they subscribe to or publish on (e.g., `string_handler.topic_name`, `laserscan_handler.topic`). These parameters are declared by the handlers themselves.

### 6. Observable Interaction Scenarios

*   **ROS 2 Message to Network**:
    1.  A message is published on a ROS 2 topic (e.g., `/my_topic`).
    2.  A Modular GW node (client or server) is running with a message handler configured for the message type of `/my_topic` and set to `SUBSCRIBER_ONLY` or `BOTH` mode, and its internal topic parameter matches `/my_topic`.
    3.  The gateway node, if connected to a peer, will send a network message.
    4.  The network message will have a header as described in section 3.1, with the `Topic Name` field set to `/my_topic`, the `Message Type` corresponding to the ROS 2 message, and the `Payload` containing the (likely serialized) ROS 2 message data. The `id_group` and `identifier_in_group` will match those configured for the sending gateway node.

*   **Network Message to ROS 2**:
    1.  A Modular GW node (client or server) receives a network message from a peer that conforms to the protocol in section 3.1.
    2.  The gateway node has a message handler registered that `can_process_message_type` matching the `Message Type` in the received network header, and this handler is configured in `PUBLISHER_ONLY` or `BOTH` mode.
    3.  The handler will attempt to deserialize the `Payload` (if the `Serialized` flag is set) into the corresponding ROS 2 message type.
    4.  The handler will then publish this ROS 2 message on a ROS 2 topic. The specific topic it publishes to is determined by the handler's internal configuration (often matching the `Topic Name` from the network header, or a remapped topic).

### 7. Network Connection Behavior

*   **Client (`gateway_client`)**:
    *   On startup (if `auto_start_receiver` is true), attempts to connect to the configured `server_host` and `server_port`.
    *   If connection fails, it may retry based on `max_retries` and `connection_timeout_ms`.
    *   If the connection is lost, it will typically attempt to reconnect.
*   **Server (`gateway_server`)**:
    *   On startup (if `auto_start_receiver` is true), starts listening on the configured `server_port`.
    *   Accepts one incoming TCP connection at a time. If a client disconnects, the server usually goes back to listening for a new client.

Messages are only exchanged when a valid TCP connection is established between a client and a server. If the connection drops, message transmission ceases until it's re-established.