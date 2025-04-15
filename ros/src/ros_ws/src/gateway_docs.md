Collecting workspace information# Modular Gateway Sender Documentation

## Overview

The Modular Gateway Sender is a ROS2 framework designed to transmit ROS messages over network protocols (currently TCP, with UDP planned). It uses a modular architecture that separates concerns into distinct components:

1. **Transport Layer** - Handles network communication
2. **Gateway Base** - Common functionality for all gateway modules
3. **Gateway Modules** - Type-specific implementations for different message types

## Architecture Components

### 1. Transport Layer (`transport_base.hpp`)

The transport layer defines an abstract interface (`TransportBase`) that network protocols must implement:

```cpp
class TransportBase {
public:
  virtual ~TransportBase() = default;
  virtual bool connect() = 0;
  virtual void disconnect() = 0;
  virtual bool send_data(const void* data, size_t size) = 0;
  virtual bool is_connected() const = 0;
};
```

Currently, the system provides:
- `TcpTransport`: A TCP implementation that handles connections to a remote server

### 2. Message Protocol (`message_header.hpp`)

Messages are framed with a binary header containing:
- Magic bytes (`0xA5C3`) for synchronization
- Flags for optional features (timestamps, fragmentation, etc.)
- Message type information
- Payload size
- Topic information

The protocol supports various message types:
```cpp
enum class MessageType : uint8_t {
  STRING = 1,
  INT32 = 2,
  FLOAT32 = 3,
  BOOL = 4,
  IMAGE = 5,
  // More can be added
};
```

### 3. Base Gateway (`ros_gateway.hpp`, ros_gateway.cpp)

The `RosGateway` class provides:
- Parameter declaration and parsing
- Transport initialization based on parameters
- Message sending with appropriate headers
- Connection management

### 4. Gateway Modules (`string_gateway.cpp`)

Gateway modules inherit from `RosGateway` and:
- Subscribe to specific ROS message types
- Convert ROS messages to the gateway format
- Send converted messages through the transport layer

## Adding New Gateway Modules

To add a new gateway module for a different message type, follow these steps:

### 1. Create a new `.cpp` file for your gateway module

Create a new file (e.g., `your_type_gateway.cpp`) in the `src` directory:

```cpp
#include "modular_gateway_sender/ros_gateway.hpp"
#include "your_ros_msg_type/msg/your_type.hpp"

namespace gateway {

class YourTypeGateway : public RosGateway {
public:
  YourTypeGateway() : RosGateway("your_type_gateway")
  {
    // Declare parameters
    declare_parameter("ros_topic", "default_topic");
    std::string ros_topic = get_parameter("ros_topic").as_string();
    
    // Create ROS subscription
    subscription_ = create_subscription<your_ros_msg_type::msg::YourType>(
      ros_topic, 10,
      [this, ros_topic](your_ros_msg_type::msg::YourType::UniquePtr msg) {
        LOG_INFO(this->get_logger(), "Received message from ROS");
        this->handle_your_type_message(ros_topic, msg);
      }
    );
  }

private:
  void handle_your_type_message(const std::string& topic, 
                              const your_ros_msg_type::msg::YourType::UniquePtr& msg)
  {
    // Convert your message to binary format
    // ... conversion code here ...
    
    // Determine appropriate MessageType enum value
    MessageType type = MessageType::YOUR_TYPE; // You may need to add this to message_header.hpp
    
    // Send the message
    send_message(topic, type, your_data_buffer, buffer_size);
  }

  // ROS subscription
  rclcpp::Subscription<your_ros_msg_type::msg::YourType>::SharedPtr subscription_;
};

} // namespace gateway

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<gateway::YourTypeGateway>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
```

### 2. Add your message type to `message_header.hpp` (if needed)

If your message type doesn't fit into existing types, add it to the `MessageType` enum in `message_header.hpp`:

```cpp
enum class MessageType : uint8_t {
  STRING = 1,
  INT32 = 2,
  FLOAT32 = 3,
  BOOL = 4,
  IMAGE = 5,
  YOUR_TYPE = 6,  // Add your new type here
  // Add more as needed
};
```

### 3. Update `CMakeLists.txt` to build your new gateway

Add your new gateway to the `CMakeLists.txt`:

```cmake
# Add your gateway executable
add_executable(your_type_gateway src/your_type_gateway.cpp src/ros_gateway.cpp src/tcp_transport.cpp)
ament_target_dependencies(your_type_gateway rclcpp your_ros_msg_type)

# Install your executable
install(TARGETS
  your_type_gateway
  DESTINATION lib/${PROJECT_NAME}
)
```

### 4. Add any needed dependencies to `package.xml`

```xml
<depend>your_ros_msg_type</depend>
```

## Key Configuration Parameters

Each gateway module accepts the following parameters:

- `transport_type`: Transport protocol to use ("tcp" or "udp" - though UDP is not yet implemented)
- `server_host`: Host address of the receiving server
- `server_port`: Port number of the receiving server
- `max_retries`: Number of connection retry attempts
- `ros_topic`: ROS topic to subscribe to (specific to each gateway)

## Building and Running

To build the modular gateway sender:

```bash
cd /home/ubuntu/ros_ws/
colcon build --packages-select modular_gateway_sender
```

To run a specific gateway module:

```bash
ros2 run modular_gateway_sender string_gateway --ros-args -p ros_topic:=my_string_topic -p server_host:=192.168.1.100
