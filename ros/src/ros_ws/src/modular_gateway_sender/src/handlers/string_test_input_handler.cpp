#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/handlers/string_test_input_handler.hpp" // Corrected include
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

StringTestInputHandler::StringTestInputHandler(RosGateway* gateway)
  : MessageHandlerBase(gateway, "string_test_input_handler") // Changed handler name
{
}

void StringTestInputHandler::initialize()
{
  // Get parameters
  // Suggesting a different default topic and parameter name for clarity
  gateway_->declare_parameter("string_test_input_handler.topic", "test_input_topic"); 
  topic_name_ = gateway_->get_parameter("string_test_input_handler.topic").as_string();
  
  // Create subscription
  subscription_ = gateway_->create_subscription<std_msgs::msg::String>(
    topic_name_, 10,
    [this](const std_msgs::msg::String::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
  LOG_INFO(gateway_->get_logger(), "StringTestInputHandler initialized for topic: %s", topic_name_.c_str());
}

void StringTestInputHandler::shutdown()
{
  subscription_.reset();
  publishers_.clear();
  enabled_ = false; // Ensure handler is marked as disabled
  LOG_INFO(gateway_->get_logger(), "StringTestInputHandler for topic %s shut down", topic_name_.c_str());
}

void StringTestInputHandler::handle_message(const std::string& topic, 
                                  const std_msgs::msg::String::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  LOG_INFO(gateway_->get_logger(), "StringTestInputHandler received on topic '%s': %s", topic.c_str(), msg->data.c_str());

  MessageOptions options;
  // If you need specific options for this message type, set them here.
  // For example, if these messages should always be marked as serialized (even if they aren't currently):
  // options.serialized = true; 
  
  // Send message with the correct MessageType
  send_message(topic, MessageType::STRING_TEST_INPUT, msg->data.c_str(), msg->data.size(), options);
}

bool StringTestInputHandler::process_and_publish_received_msg(
  const std::string& topic,
  MessageType type,
  const void* data,
  size_t size,
  const MessageOptions& options)
{
  if (!is_enabled() || !is_ros_publisher_enabled() || !can_process_message_type(type)) { // Added publisher_enabled check
    return false;
  }

  auto it = publishers_.find(topic);
  if (it == publishers_.end()) {
    auto publisher = gateway_->create_publisher<std_msgs::msg::String>(topic, 10);
    it = publishers_.emplace(topic, publisher).first;
    RCLCPP_INFO(gateway_->get_logger(), "StringTestInputHandler created publisher for topic: %s", topic.c_str());
  }

  if (options.serialized) {
    LOG_ERROR(gateway_->get_logger(), "StringTestInputHandler: Serialized string_test_input messages are not supported for processing");
    // Potentially return false or handle differently if serialized messages are expected for this type
  }
  else {
    std_msgs::msg::String msg;
    msg.data = std::string(static_cast<const char*>(data), size);
    it->second->publish(msg);
    
    RCLCPP_INFO(gateway_->get_logger(), "StringTestInputHandler published to topic %s: %s", 
              topic.c_str(), msg.data.c_str());
  }

  return true;
}

} // namespace gateway