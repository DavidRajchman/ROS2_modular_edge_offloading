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
}

void StringHandler::handle_message(const std::string& topic, 
                                  const std_msgs::msg::String::SharedPtr msg)
{
  if (!is_enabled()) return;
  
  LOG_INFO(gateway_->get_logger(), "StringHandler received: %s", msg->data.c_str());

  MessageOptions options;
  
  
  // Send message with default options (no flags)
  send_message(topic, MessageType::STRING, msg->data.c_str(), msg->data.size(),options);
}

} // namespace gateway