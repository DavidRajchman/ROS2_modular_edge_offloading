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
  LOG_INFO(gateway_->get_logger(), "LaserScanHandler shut down");
}

void LaserScanHandler::handle_message(const std::string& topic, 
                                     const sensor_msgs::msg::LaserScan::SharedPtr msg)
{
  if (!is_enabled()) return;
  
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

} // namespace gateway