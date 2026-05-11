
#include "modular_gateway_sender/handlers/joint_state_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "rclcpp/serialization.hpp"

//Robotic arm state information handler
namespace gateway {

// Constructor: Initialize handler for sensor_msgs/msg/JointState
// Handles continuous joint coordinate data published by the RoArm sliders
JointStateHandler::JointStateHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node)
  : MessageHandlerBase(gateway, node, "joint_state_handler"),
    logger_(CppLogging::Logger("gateway"))
{
}

// Initialize ROS subscription for local JointState messages
void JointStateHandler::initialize()
{
  node_->declare_parameter("joint_state_handler.topic", "joint_states");
  topic_name_ = node_->get_parameter("joint_state_handler.topic").as_string();
  
  subscription_ = node_->create_subscription<sensor_msgs::msg::JointState>(
    topic_name_, 10,
    [this](const sensor_msgs::msg::JointState::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
  
  logger_.Info("joint_state_handler.cpp: Initialized on topic: {}", topic_name_);
}

// Clean up subscriptions and publishers
void JointStateHandler::shutdown()
{
  subscription_.reset();
  publishers_.clear();
  logger_.Info("joint_state_handler.cpp: Shut down");
}

bool JointStateHandler::can_process_message_type(MessageType type) const
{
  return type == MessageType::smJOINTSTATE; 
}

// Handle incoming ROS JointState message from local subscription
// Uses ROS serialization (CDR format) for efficient binary transmission
void JointStateHandler::handle_message(const std::string& topic, 
                                     const sensor_msgs::msg::JointState::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  // Serialize JointState to CDR binary format for transmission
  rclcpp::Serialization<sensor_msgs::msg::JointState> serialization;
  rclcpp::SerializedMessage serialized_msg;
  
  serialization.serialize_message(msg.get(), &serialized_msg);
  
  MessageOptions options;
  options.serialized = true;        // Indicates CDR serialized data
  options.has_timestamp = true;     // JointState contains a header timestamp
  
  const void* data = serialized_msg.get_rcl_serialized_message().buffer;
  size_t size = serialized_msg.get_rcl_serialized_message().buffer_length;
  
  send_message(topic, MessageType::smJOINTSTATE, data, size, options);
  
  logger_.Info("joint_state_handler.cpp: Sent JointState message, {} joints", msg->name.size());
}

// Process received data plane message and publish to local ROS topic
// Deserializes CDR binary format back to JointState message
bool JointStateHandler::process_and_publish_received_msg(
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
      auto publisher = node_->create_publisher<sensor_msgs::msg::JointState>(topic, 10);
      it = publishers_.emplace(topic, publisher).first;
      logger_.Info("joint_state_handler.cpp: Created JointState publisher for topic: {}", topic);
    }

    // JointState must be serialized (CDR format)
    if (options.serialized == false) {
      logger_.Error("joint_state_handler.cpp: Received JointState message without serialization flag");
      return false;
    }
    
    // Copy received data into ROS serialized message wrapper
    rclcpp::SerializedMessage serialized_msg(size);
    memcpy(serialized_msg.get_rcl_serialized_message().buffer, data, size);
    serialized_msg.get_rcl_serialized_message().buffer_length = size;
    
    // Deserialize from CDR to JointState message
    sensor_msgs::msg::JointState joint_msg;
    rclcpp::Serialization<sensor_msgs::msg::JointState> serialization;
    serialization.deserialize_message(&serialized_msg, &joint_msg);

    it->second->publish(joint_msg);
    
    logger_.Info("joint_state_handler.cpp: Published JointState to topic {}: {} joints", 
              topic, joint_msg.name.size());

    return true;
  }
  catch (const std::exception& e) {
    logger_.Error("joint_state_handler.cpp: Error processing JointState message: {}", e.what());
    return false;
  }
}

} // namespace gateway