#ifndef LASERSCAN_HANDLER_HPP
#define LASERSCAN_HANDLER_HPP

#include "modular_gateway_sender/message_handler_base.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "logging/logger.h"
#include <map>

namespace gateway {

class LaserScanHandler : public MessageHandlerBase {
public:
  LaserScanHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node);
  
  void initialize() override;
  void shutdown() override;
  
  bool can_process_message_type(MessageType type) const override {
    return type == MessageType::smLASERSCAN;
  }
  
  bool process_and_publish_received_msg(
      const std::string& topic,
      MessageType type,
      const void* data,
      size_t size,
      const MessageOptions& options) override;
  
private:
  void handle_message(const std::string& topic, const sensor_msgs::msg::LaserScan::SharedPtr msg);
  
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr subscription_;
  std::string topic_name_;
  bool empty_intensities_;
  
  std::map<std::string, rclcpp::Publisher<sensor_msgs::msg::LaserScan>::SharedPtr> publishers_;
  CppLogging::Logger logger_;
};

} // namespace gateway

#endif // LASERSCAN_HANDLER_HPP