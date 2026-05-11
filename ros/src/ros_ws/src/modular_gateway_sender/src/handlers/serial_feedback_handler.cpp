#include "modular_gateway_sender/handlers/serial_feedback_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"
#include "std_msgs/msg/string.hpp"

namespace gateway {

// Constructor: Initialize handler for std_msgs/msg/String
// Handles the JSON telemetry and physical feedback published by the RoArm to /serial_ctrl/rx
SerialFeedbackHandler::SerialFeedbackHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node)
  : MessageHandlerBase(gateway, node, "serial_feedback_handler"),
    logger_(CppLogging::Logger("gateway"))
{
}

// Initialize ROS subscription for local string messages (listening to the arm)
void SerialFeedbackHandler::initialize()
{
  node_->declare_parameter("serial_feedback_handler.topic", "serial_ctrl/rx"); 
  topic_name_ = node_->get_parameter("serial_feedback_handler.topic").as_string();
  
  subscription_ = node_->create_subscription<std_msgs::msg::String>(
    topic_name_, 10,
    [this](const std_msgs::msg::String::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
  logger_.Info("serial_feedback_handler.cpp: Initialized for topic: {}", topic_name_);
}

// Clean up subscriptions and publishers
void SerialFeedbackHandler::shutdown()
{
  subscription_.reset();
  publishers_.clear();
  enabled_ = false;
  logger_.Info("serial_feedback_handler.cpp: For topic {} shut down", topic_name_);
}

bool SerialFeedbackHandler::can_process_message_type(MessageType type) const
{
  // Requires a distinct MessageType so TX commands and RX feedback don't cross paths
  return type == MessageType::smSERIAL_FEEDBACK; 
}

// Handle incoming ROS string message from the physical arm
// Sends raw telemetry data Uplink over the 5G network to the Edge Server
void SerialFeedbackHandler::handle_message(const std::string& topic, 
                                  const std_msgs::msg::String::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  MessageOptions options;  
  options.serialized = false; // Sending as raw char array to save bandwidth
  
  send_message(topic, MessageType::smSERIAL_FEEDBACK, msg->data.c_str(), msg->data.size(), options);
  
  logger_.Info("serial_feedback_handler.cpp: Sent telemetry Uplink on {}: {}", topic, msg->data);
}

// Process received data plane message and publish to local ROS topic
// Reconstructs raw char buffer back into a standard ROS String on the Edge Server
bool SerialFeedbackHandler::process_and_publish_received_msg(
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
    auto it = publishers_.find(topic);
    if (it == publishers_.end()) {
      auto publisher = node_->create_publisher<std_msgs::msg::String>(topic, 10);
      it = publishers_.emplace(topic, publisher).first;
      logger_.Info("serial_feedback_handler.cpp: Created feedback publisher for topic: {}", topic);
    }

    if (options.serialized) {
      logger_.Error("serial_feedback_handler.cpp: Received CDR serialized string, expected raw string");
      return false;
    }
    
    std_msgs::msg::String msg;
    msg.data = std::string(static_cast<const char*>(data), size);
    
    it->second->publish(msg);
    
    logger_.Info("serial_feedback_handler.cpp: Published telemetry to topic {}: {}", topic, msg.data);

    return true;
  }
  catch (const std::exception& e) {
    logger_.Error("serial_feedback_handler.cpp: Error processing feedback message: {}", e.what());
    return false;
  }
}

} // namespace gateway