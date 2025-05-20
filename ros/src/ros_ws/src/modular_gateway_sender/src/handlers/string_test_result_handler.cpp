#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/handlers/string_test_result_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

StringTestResultHandler::StringTestResultHandler(RosGateway* gateway)
  : MessageHandlerBase(gateway, "string_test_result_handler")
{
}

void StringTestResultHandler::initialize()
{
  gateway_->declare_parameter("string_test_result_handler.topic", "test_result_topic"); 
  topic_name_ = gateway_->get_parameter("string_test_result_handler.topic").as_string();
  
  subscription_ = gateway_->create_subscription<std_msgs::msg::String>(
    topic_name_, 10,
    [this](const std_msgs::msg::String::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
  LOG_INFO(gateway_->get_logger(), "StringTestResultHandler initialized for topic: %s", topic_name_.c_str());
}

void StringTestResultHandler::shutdown()
{
  subscription_.reset();
  publishers_.clear();
  enabled_ = false;
  LOG_INFO(gateway_->get_logger(), "StringTestResultHandler for topic %s shut down", topic_name_.c_str());
}

void StringTestResultHandler::handle_message(const std::string& topic, 
                                  const std_msgs::msg::String::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  LOG_INFO(gateway_->get_logger(), "StringTestResultHandler received on topic '%s': %s", topic.c_str(), msg->data.c_str());

  MessageOptions options;
  // Configure options if needed, e.g., options.serialized = true;
  
  send_message(topic, MessageType::STRING_TEST_RESULT, msg->data.c_str(), msg->data.size(), options);
}

bool StringTestResultHandler::process_and_publish_received_msg(
  const std::string& topic,
  MessageType type,
  const void* data,
  size_t size,
  const MessageOptions& options)
{
  if (!is_enabled() || !is_ros_publisher_enabled() || !can_process_message_type(type)) {
    return false;
  }

  auto it = publishers_.find(topic);
  if (it == publishers_.end()) {
    auto publisher = gateway_->create_publisher<std_msgs::msg::String>(topic, 10);
    it = publishers_.emplace(topic, publisher).first;
    RCLCPP_INFO(gateway_->get_logger(), "StringTestResultHandler created publisher for topic: %s", topic.c_str());
  }

  if (options.serialized) {
    LOG_ERROR(gateway_->get_logger(), "StringTestResultHandler: Serialized string_test_result messages are not supported for processing");
    // Handle differently if serialized messages are expected
  }
  else {
    std_msgs::msg::String msg;
    msg.data = std::string(static_cast<const char*>(data), size);
    it->second->publish(msg);
    
    RCLCPP_INFO(gateway_->get_logger(), "StringTestResultHandler published to topic %s: %s", 
              topic.c_str(), msg.data.c_str());
  }

  return true;
}

} // namespace gateway