#include "modular_gateway_sender/handlers/serial_string_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"
#include "std_msgs/msg/string.hpp"


//Robotic arm control command handler
namespace gateway {

// Constructor: Initialize handler for std_msgs/msg/String
// Handles the JSON string payloads sent to /serial_ctrl/tx for the RoArm
SerialStringHandler::SerialStringHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node)
  : MessageHandlerBase(gateway, node, "serial_string_handler"),
    logger_(CppLogging::Logger("gateway"))
{
}

// Initialize ROS subscription for local string messages
void SerialStringHandler::initialize()
{
  node_->declare_parameter("serial_string_handler.topic", "serial_ctrl/tx"); 
  topic_name_ = node_->get_parameter("serial_string_handler.topic").as_string();
  
  subscription_ = node_->create_subscription<std_msgs::msg::String>(
    topic_name_, 10,
    [this](const std_msgs::msg::String::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
  logger_.Info("serial_string_handler.cpp: Initialized for topic: {}", topic_name_);
}

// Clean up subscriptions and publishers
void SerialStringHandler::shutdown()
{
  subscription_.reset();
  publishers_.clear();
  enabled_ = false;
  logger_.Info("serial_string_handler.cpp: For topic {} shut down", topic_name_);
}

bool SerialStringHandler::can_process_message_type(MessageType type) const
{
  return type == MessageType::smSERIAL_STRING; 
}

// Handle incoming ROS string message from local topic
// Sends raw string data directly over the network to bypass CDR overhead
void SerialStringHandler::handle_message(const std::string& topic, 
                                  const std_msgs::msg::String::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  MessageOptions options;  
  options.serialized = false; // Sending as raw char array
  
  send_message(topic, MessageType::smSERIAL_STRING, msg->data.c_str(), msg->data.size(), options);
  
  logger_.Info("serial_string_handler.cpp: Sent string command on {}: {}", topic, msg->data);
}

// Process received data plane message and publish to local ROS topic
// Reconstructs raw char buffer back into a standard ROS String
bool SerialStringHandler::process_and_publish_received_msg(
  const std::string& topic,
  MessageType type,
  const void* data,
  size_t size,
  const MessageOptions& options)
{
  if (!is_enabled() || !is_ros_publisher_enabled() || !can_process_message_type(type)) {
    return false;
  }

  try {
    // Create publisher for this topic if first time receiving on it
    auto it = publishers_.find(topic);
    if (it == publishers_.end()) {
      auto publisher = node_->create_publisher<std_msgs::msg::String>(topic, 10);
      it = publishers_.emplace(topic, publisher).first;
      logger_.Info("serial_string_handler.cpp: Created publisher for topic: {}", topic);
    }

    if (options.serialized) {
      logger_.Error("serial_string_handler.cpp: Received CDR serialized string, expected raw string");
      return false;
    }
    
    // Convert raw char buffer directly back to std_msgs::msg::String
    std_msgs::msg::String msg;
    msg.data = std::string(static_cast<const char*>(data), size);
    
    it->second->publish(msg);
    
    logger_.Info("serial_string_handler.cpp: Published to topic {}: {}", topic, msg.data);

    return true;
  }
  catch (const std::exception& e) {
    logger_.Error("serial_string_handler.cpp: Error processing string message: {}", e.what());
    return false;
  }
}

} // namespace gateway