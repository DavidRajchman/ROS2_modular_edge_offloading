#ifndef LASERSCAN_HANDLER_HPP
#define LASERSCAN_HANDLER_HPP

#include "modular_gateway_sender/message_handler_base.hpp"
#include "sensor_msgs/msg/laser_scan.hpp"
#include "rclcpp/serialization.hpp"

namespace gateway {

class LaserScanHandler : public MessageHandlerBase {
public:
  LaserScanHandler(RosGateway* gateway);
  
  void initialize() override;
  void shutdown() override;
  
private:
  void handle_message(const std::string& topic, 
                     const sensor_msgs::msg::LaserScan::SharedPtr msg);
  
  rclcpp::Subscription<sensor_msgs::msg::LaserScan>::SharedPtr subscription_;
  std::string topic_name_;
  bool empty_intensities_;
};

} // namespace gateway

#endif // LASERSCAN_HANDLER_HPP