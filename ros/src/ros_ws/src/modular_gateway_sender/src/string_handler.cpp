// string_handler.cpp
#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/string_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

StringHandler::StringHandler(RosGateway* gateway)
  : MessageHandlerBase(gateway, "string_handler")
{
}

void StringHandler::initialize()
{
  // Get parameters
  gateway_->declare_parameter("string_handler.topic", "topic");
  topic_name_ = gateway_->get_parameter("string_handler.topic").as_string();
  
  // Create subscription
  subscription_ = gateway_->create_subscription<std_msgs::msg::String>(
    topic_name_, 10,
    [this](const std_msgs::msg::String::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
}

void StringHandler::shutdown()
{
  subscription_.reset();
  
  // Clean up the publishers
  publishers_.clear();
  
  // Disable the handler
  enabled_ = false;
}

void StringHandler::handle_message(const std::string& topic, 
                                  const std_msgs::msg::String::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  LOG_INFO(gateway_->get_logger(), "StringHandler received: %s", msg->data.c_str());

  MessageOptions options;
  
  
  // Send message with default options (no flags)
  send_message(topic, MessageType::STRING, msg->data.c_str(), msg->data.size(),options);
}

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

  // Find or create publisher for this topic
  auto it = publishers_.find(topic);
  if (it == publishers_.end()) {
    auto publisher = gateway_->create_publisher<std_msgs::msg::String>(topic, 10);
    it = publishers_.emplace(topic, publisher).first;
    RCLCPP_INFO(gateway_->get_logger(), "Created string publisher for topic: %s", topic.c_str());
  }

  // Process based on serialization flag
  if (options.serialized) {
    //serialization not implemented for string messages
    LOG_ERROR(gateway_->get_logger(), "Serialized string messages are not supported for processing");
  }
  else {
    // Handle raw string data
    std_msgs::msg::String msg;
    msg.data = std::string(static_cast<const char*>(data), size);
    it->second->publish(msg);
    
    RCLCPP_INFO(gateway_->get_logger(), "Published string to topic %s: %s", 
              topic.c_str(), msg.data.c_str());
  }

return true;
}

} // namespace gateway