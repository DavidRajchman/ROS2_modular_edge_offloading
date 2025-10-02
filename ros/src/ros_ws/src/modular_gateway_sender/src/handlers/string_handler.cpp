#include "modular_gateway_sender/handlers/string_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

// Constructor: Initialize handler for std_msgs/msg/String (MessageType::STRING = 1)
// Handles basic string messages, typically used for simple text data exchange
StringHandler::StringHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node)
  : MessageHandlerBase(gateway, node, "string_handler"),
    logger_(CppLogging::Logger("gateway"))
{
}

// Initialize ROS subscription for local string messages
// Called by controller after handler creation
// Creates subscriber on configured topic (default "topic")
void StringHandler::initialize()
{
  node_->declare_parameter("string_handler.topic", "topic");
  topic_name_ = node_->get_parameter("string_handler.topic").as_string();
  
  subscription_ = node_->create_subscription<std_msgs::msg::String>(
    topic_name_, 10,
    [this](const std_msgs::msg::String::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
  logger_.Info("string_handler.cpp: Initialized for topic: {}", topic_name_);
}

// Clean up subscriptions and publishers
// Called during session termination or handler deactivation
void StringHandler::shutdown()
{
  subscription_.reset();
  publishers_.clear();
  enabled_ = false;
  logger_.Info("string_handler.cpp: Shut down for topic {}", topic_name_);
}

// Handle incoming ROS string message from local subscription
// Called by ROS subscriber callback when message arrives on local topic
// Sends raw string data (not serialized) to remote via data plane
void StringHandler::handle_message(const std::string& topic, 
                                  const std_msgs::msg::String::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  logger_.Info("string_handler.cpp: Received: {}", msg->data);

  MessageOptions options;  // serialized=false for raw string data
  
  send_message(topic, MessageType::STRING, msg->data.c_str(), msg->data.size(), options);
}

// Process received data plane message and publish to local ROS topic
// Called by RosGateway when data plane frame arrives with MessageType::STRING
// Creates publisher on-demand if not already exists for topic
// Expects raw string data (not ROS serialized)
bool StringHandler::process_and_publish_received_msg(
  const std::string& topic,
  MessageType type,
  const void* data,
  size_t size,
  const MessageOptions& options)
{
  if (!is_enabled() || !can_process_message_type(type)) {
    return false;
  }

  // Create publisher for this topic if first time receiving on it
  auto it = publishers_.find(topic);
  if (it == publishers_.end()) {
    auto publisher = node_->create_publisher<std_msgs::msg::String>(topic, 10);
    it = publishers_.emplace(topic, publisher).first;
    logger_.Info("string_handler.cpp: Created string publisher for topic: {}", topic);
  }

  if (options.serialized) {
    logger_.Error("string_handler.cpp: Serialized string messages are not supported for processing");
  }
  else {
    // Convert raw char buffer to std_msgs::msg::String and publish
    std_msgs::msg::String msg;
    msg.data = std::string(static_cast<const char*>(data), size);
    it->second->publish(msg);
    
    logger_.Info("string_handler.cpp: Published string to topic {}: {}", 
              topic, msg.data);
  }

  return true;
}

} // namespace gateway