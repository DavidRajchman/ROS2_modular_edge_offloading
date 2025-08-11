#include "modular_gateway_sender/handlers/string_test_input_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

StringTestInputHandler::StringTestInputHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node)
  : MessageHandlerBase(gateway, node, "string_test_input_handler"),
    logger_(CppLogging::Logger("gateway"))
{
}

void StringTestInputHandler::initialize()
{
  // Updated default to align with task database and VHC publisher topic
  node_->declare_parameter("string_test_input_handler.topic", "test/string_input"); 
  topic_name_ = node_->get_parameter("string_test_input_handler.topic").as_string();
  
  subscription_ = node_->create_subscription<std_msgs::msg::String>(
    topic_name_, 10,
    [this](const std_msgs::msg::String::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
  logger_.Info("string_test_input_handler.cpp: Initialized for topic: {}", topic_name_);
}

void StringTestInputHandler::shutdown()
{
  subscription_.reset();
  publishers_.clear();
  enabled_ = false;
  logger_.Info("string_test_input_handler.cpp: For topic {} shut down", topic_name_);
}

void StringTestInputHandler::handle_message(const std::string& topic, 
                                  const std_msgs::msg::String::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  logger_.Info("string_test_input_handler.cpp: Received on topic '{}': {}", topic, msg->data);

  MessageOptions options;
  
  send_message(topic, MessageType::STRING_TEST_INPUT, msg->data.c_str(), msg->data.size(), options);
}

bool StringTestInputHandler::process_and_publish_received_msg(
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
    auto publisher = node_->create_publisher<std_msgs::msg::String>(topic, 10);
    it = publishers_.emplace(topic, publisher).first;
    logger_.Info("string_test_input_handler.cpp: Created publisher for topic: {}", topic);
  }

  if (options.serialized) {
    logger_.Error("string_test_input_handler.cpp: Serialized messages are not supported for processing");
  }
  else {
    std_msgs::msg::String msg;
    msg.data = std::string(static_cast<const char*>(data), size);
    it->second->publish(msg);
    
    logger_.Info("string_test_input_handler.cpp: Published to topic {}: {}", 
              topic, msg.data);
  }

  return true;
}

} // namespace gateway