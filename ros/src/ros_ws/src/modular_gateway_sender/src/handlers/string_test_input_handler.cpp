#include "modular_gateway_sender/handlers/string_test_input_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

// Constructor: Initialize handler for test/string_input (MessageType::STRING_TEST_INPUT = 201)
// Test handler for VHC→MEC offloading experiments with string input data
StringTestInputHandler::StringTestInputHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node)
  : MessageHandlerBase(gateway, node, "string_test_input_handler"),
    logger_(CppLogging::Logger("gateway"))
{
}

// Initialize ROS subscription for local test input messages
// Called by controller after handler creation
// Default topic "test/string_input" matches task database configuration
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

// Clean up subscriptions and publishers
// Called during session termination or handler deactivation
void StringTestInputHandler::shutdown()
{
  subscription_.reset();
  publishers_.clear();
  enabled_ = false;
  logger_.Info("string_test_input_handler.cpp: For topic {} shut down", topic_name_);
}

// Handle incoming ROS string message from local test topic
// Called by ROS subscriber callback when test input arrives
// Sends raw string data to remote MEC for processing
void StringTestInputHandler::handle_message(const std::string& topic, 
                                  const std_msgs::msg::String::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  logger_.Info("string_test_input_handler.cpp: Received on topic '{}': {}", topic, msg->data);

  MessageOptions options;  // serialized=false for raw string data
  
  send_message(topic, MessageType::STRING_TEST_INPUT, msg->data.c_str(), msg->data.size(), options);
}

// Process received data plane message and publish to local ROS topic
// Called by RosGateway when data plane frame arrives with MessageType::STRING_TEST_INPUT
// Used on MEC side to receive test input from VHC
// Creates publisher on-demand if not already exists for topic
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

  // Create publisher for this topic if first time receiving on it
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
    // Convert raw char buffer to std_msgs::msg::String and publish
    std_msgs::msg::String msg;
    msg.data = std::string(static_cast<const char*>(data), size);
    it->second->publish(msg);
    
    logger_.Info("string_test_input_handler.cpp: Published to topic {}: {}", 
              topic, msg.data);
  }

  return true;
}

} // namespace gateway