Collecting workspace information# Modular Gateway Documentation (v0.12.1)

## Overview

Modular Gateway provides a flexible system for sending topic messages between different ROS2 environments over a network. The architecture is designed to be extensible with a clear separation of concerns.

## Architecture

The system consists of three main components:

1. **Topic Handlers** - Classes derived from `MessageHandlerBase` that handle topic subscriptions and publications. Each handler can:
   - Subscribe to ROS2 topics and relay messages to the network
   - Receive messages from the network and publish to ROS2 topics
   - Optionally transform messages (like removing LaserScan intensities to reduce size)

2. **Gateway Core** - The `RosGateway` class is the central component that:
   - Manages handler registration and configuration
   - Provides a unified interface for sending messages
   - Controls the active transport layer
   - Manages the receiver thread for incoming messages

3. **Transport Layer** - Implemented through the `TransportBase` abstraction:
   - `TcpClientTransport` for connecting to a server
   - `TcpServerTransport` for accepting connections
   - Easily extensible to new protocols (e.g., UDP) by implementing the base interface

## Message Protocol

The system uses a binary protocol defined in message_header.hpp:

- **Header Structure**:
  - Magic bytes (2 bytes): 0xA5C3 to identify valid messages
  - Flags byte (1 byte): Holds metadata like serialization status
  - Message type (1 byte): Identifies content type (strings, laser scans, etc.)
  - Payload size (4 bytes): Length of message body
  - Topic length (1 byte): Length of topic string
  - Topic name (variable): The ROS2 topic
  - Payload: The actual message data

- **Message Options** provides a user-friendly way to configure advanced features:
  - Timestamps
  - Message fragmentation
  - Serialization flags
  - Data compression

## Modes of Operation

The every topic handler supports two operating modes, controlled via `HandlerMode` enum using helper function `configure_handler_mode`:

1. **SUBSCRIBER_ONLY** - Subscribes to ROS topics and sends to the network
2. **PUBLISHER_ONLY** - Receives from network and publishes to ROS topics
3. **BOTH** - Operates in both modes simultaneously - DO NOT USE in current implementation - creates infinite message loops

These modes can be configured independently for each message handler using the helper function, or directly by using the control methods 
- `enable_ros_subscriber_mode()` 
- `disable_ros_subscriber_mode()`
- `enable_ros_pubslisher_mode()`
- `disable_ros_pubslisher_mode()`

## Nodes

Two complementary node types support different network configurations:

- **Client Mode** (`gateway_client_main.cpp`): Connects to a server
- **Server Mode** (`gateway_server_main.cpp`): Accepts connections


## Extending with New Handlers 

To add a new message handler for a custom type:

1. Create a class that inherits from `MessageHandlerBase`
2. Implement the required virtual methods:
   - `initialize()` - Create subscriptions or publishers
   - `shutdown()` - Clean up resources
   - `can_process_message_type()` - Check if the handler supports a message type
   - `process_and_publish_received_msg()` - Process incoming messages from network

3. Register the message type in `MessageType` enum
4. Add the handler to the appropriate main file

## Thread Safety

The system uses mutex protection with the `transport_access_mutex_` to ensure thread-safe access to the transport layer when sending and receiving messages.

## Connection Management

Both `TcpClientTransport` and `TcpServerTransport` implement:

- Automatic reconnection logic
- Timeout handling 
- Non-blocking I/O
- Error recovery

The `RosGateway::receiver_thread_func()` handles receiving and processing incoming messages, with configurable polling frequency default is 1000 us .

## Usability Features

- Comprehensive logging with version information
- Parameter-based configuration through ROS2 parameter system
- Automatic start/stop of components based on node lifecycle
- Clean shutdown handling

## Performance Considerations

- Message serialization is handled in the topic handlers
- Optional compression and payload transformation (Compresion NOT YET IMPLEMENTED)
- Non-blocking I/O for transport layers
- Configurable receiver polling frequency