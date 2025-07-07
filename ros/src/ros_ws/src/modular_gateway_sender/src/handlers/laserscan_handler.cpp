#include "modular_gateway_sender/handlers/laserscan_handler.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

LaserScanHandler::LaserScanHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node)
  : MessageHandlerBase(gateway, node, "laserscan_handler"),
    logger_(CppLogging::Logger("gateway"))
{
}

void LaserScanHandler::initialize()
{
  node_->declare_parameter("laserscan_handler.topic", "scan");
  node_->declare_parameter("laserscan_handler.empty_intensities", true);
  
  topic_name_ = node_->get_parameter("laserscan_handler.topic").as_string();
  empty_intensities_ = node_->get_parameter("laserscan_handler.empty_intensities").as_bool();
  
  subscription_ = node_->create_subscription<sensor_msgs::msg::LaserScan>(
    topic_name_, 10,
    [this](const sensor_msgs::msg::LaserScan::SharedPtr msg) {
      this->handle_message(topic_name_, msg);
    }
  );
  
  logger_.Info("laserscan_handler.cpp: Initialized on topic: {}", topic_name_);
  logger_.Info("laserscan_handler.cpp: Empty intensities option: {}", empty_intensities_ ? "true" : "false");
}

void LaserScanHandler::shutdown()
{
  subscription_.reset();
  publishers_.clear();
  logger_.Info("laserscan_handler.cpp: Shut down");
}

void LaserScanHandler::handle_message(const std::string& topic, 
                                     const sensor_msgs::msg::LaserScan::SharedPtr msg)
{
  if (!is_enabled() || !is_ros_subscriber_enabled()) return;
  
  sensor_msgs::msg::LaserScan scan_msg = *msg;
  
  if (empty_intensities_ && !scan_msg.intensities.empty()) {
    logger_.Debug("laserscan_handler.cpp: Clearing intensities array");
    scan_msg.intensities.clear();
  }
  
  rclcpp::Serialization<sensor_msgs::msg::LaserScan> serialization;
  rclcpp::SerializedMessage serialized_msg;
  
  serialization.serialize_message(&scan_msg, &serialized_msg);
  
  MessageOptions options;
  options.serialized = true;
  options.has_timestamp = true;
  
  const void* data = serialized_msg.get_rcl_serialized_message().buffer;
  size_t size = serialized_msg.get_rcl_serialized_message().buffer_length;
  
  send_message(topic, MessageType::smLASERSCAN, data, size, options);
  
  logger_.Info("laserscan_handler.cpp: Sent LaserScan message, ranges: {}, intensities: {}", 
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
    auto it = publishers_.find(topic);
    if (it == publishers_.end()) {
      auto publisher = node_->create_publisher<sensor_msgs::msg::LaserScan>(topic, 10);
      it = publishers_.emplace(topic, publisher).first;
      logger_.Info("laserscan_handler.cpp: Created LaserScan publisher for topic: {}", topic);
    }

    if (options.serialized == false) {
      logger_.Error("laserscan_handler.cpp: Received LaserScan message without serialization flag");
      return false;
    }
    
    rclcpp::SerializedMessage serialized_msg(size);
    
    memcpy(serialized_msg.get_rcl_serialized_message().buffer, data, size);
    serialized_msg.get_rcl_serialized_message().buffer_length = size;
    
    sensor_msgs::msg::LaserScan laser_msg;
    rclcpp::Serialization<sensor_msgs::msg::LaserScan> serialization;
    serialization.deserialize_message(&serialized_msg, &laser_msg);

    it->second->publish(laser_msg);
    
    logger_.Info("laserscan_handler.cpp: Published LaserScan to topic {}: ranges: {}, intensities: {}", 
              topic, laser_msg.ranges.size(), laser_msg.intensities.size());

    return true;
  }
  catch (const std::exception& e) {
    logger_.Error("laserscan_handler.cpp: Error processing LaserScan message: {}", e.what());
    return false;
  }
}

} // namespace gateway