#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/laserscan_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

LaserScanHandler::LaserScanHandler(RosGateway* gateway)
  : MessageHandlerBase(gateway, "laserscan_handler")
{
}

void LaserScanHandler::initialize()
{
  // Get parameters
  gateway_->declare_parameter("laserscan_handler.topic", "scan");
  gateway_->declare_parameter("laserscan_handler.empty_intensities", true);
  
  topic_name_ = gateway_->get_parameter("laserscan_handler.topic").as_string();
  empty_intensities_ = gateway_->get_parameter("laserscan_handler.empty_intensities").as_bool();
  
  // Create subscription
  subscription_ = gateway_->create_subscription<sensor_msgs::msg::LaserScan>(
    topic_name_, 10,
    [this](const sensor_msgs::msg::LaserScan::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
  
  LOG_INFO(gateway_->get_logger(), "LaserScanHandler initialized on topic: %s", topic_name_.c_str());
  LOG_INFO(gateway_->get_logger(), "Empty intensities option: %s", empty_intensities_ ? "true" : "false");
}

void LaserScanHandler::shutdown()
{
  subscription_.reset();
  
  // Clean up publishers
  publishers_.clear();
  
  LOG_INFO(gateway_->get_logger(), "LaserScanHandler shut down");
}

void LaserScanHandler::handle_message(const std::string& topic, 
                                     const sensor_msgs::msg::LaserScan::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  // Create a copy of the message to modify if needed
  sensor_msgs::msg::LaserScan scan_msg = *msg;
  
  // Empty intensities array if configured to do so
  if (empty_intensities_ && !scan_msg.intensities.empty()) {
    LOG_DEBUG(gateway_->get_logger(), "Clearing intensities array");
    scan_msg.intensities.clear();
  }
  
  // Serialize the message
  rclcpp::Serialization<sensor_msgs::msg::LaserScan> serialization;
  rclcpp::SerializedMessage serialized_msg;
  
  serialization.serialize_message(&scan_msg, &serialized_msg);
  
  // Create message options with serialization flag set
  MessageOptions options;
  options.serialized = true;
  options.has_timestamp = true;  // Include timestamps for sensor data
  
  // Get raw buffer and size from serialized message
  const void* data = serialized_msg.get_rcl_serialized_message().buffer;
  size_t size = serialized_msg.get_rcl_serialized_message().buffer_length;
  
  // Send the serialized message
  send_message(topic, MessageType::smLASERSCAN, data, size, options);
  
  LOG_INFO(gateway_->get_logger(), "Sent LaserScan message, ranges: %zu, intensities: %zu", 
           scan_msg.ranges.size(), scan_msg.intensities.size());
}

bool LaserScanHandler::process_and_publish_received_msg(
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
    // Find or create publisher for this topic
    auto it = publishers_.find(topic);
    if (it == publishers_.end()) {
      auto publisher = gateway_->create_publisher<sensor_msgs::msg::LaserScan>(topic, 10);
      it = publishers_.emplace(topic, publisher).first;
      LOG_INFO(gateway_->get_logger(), "Created LaserScan publisher for topic: %s", topic.c_str());
    }

    if (options.serialized == false) {
      LOG_ERROR(gateway_->get_logger(), "Received LaserScan message without serialization flag");
      return false;
    }
    
    // Create a properly sized serialized message and copy the data
    rclcpp::SerializedMessage serialized_msg(size);
    
    // Copy the data instead of sharing the pointer
    memcpy(serialized_msg.get_rcl_serialized_message().buffer, data, size);
    serialized_msg.get_rcl_serialized_message().buffer_length = size;
    
    sensor_msgs::msg::LaserScan laser_msg;
    rclcpp::Serialization<sensor_msgs::msg::LaserScan> serialization;
    serialization.deserialize_message(&serialized_msg, &laser_msg);

    // Publish the message
    it->second->publish(laser_msg);
    
    LOG_INFO(gateway_->get_logger(), "Published LaserScan to topic %s: ranges: %zu, intensities: %zu", 
              topic.c_str(), laser_msg.ranges.size(), laser_msg.intensities.size());

    return true;
  }
  catch (const std::exception& e) {
    LOG_ERROR(gateway_->get_logger(), "Error processing LaserScan message: %s", e.what());
    return false;
  }
}

} // namespace gateway