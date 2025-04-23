#include "modular_gateway_sender/ros_gateway.hpp"
#include "modular_gateway_sender/string_handler.hpp"
#include "modular_gateway_sender/laserscan_handler.hpp"

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  
  // Create gateway in SERVER mode
  auto gateway = std::make_shared<gateway::RosGateway>("gateway_server", gateway::TransportMode::SERVER);
  
  // Configure handlers
  auto string_handler = std::make_shared<gateway::StringHandler>(gateway.get());
  string_handler->enable();
  string_handler->enable_ros_publisher_mode();
  string_handler->disable_ros_subscriber_mode(); 
  gateway->register_handler(string_handler);
  
  auto laser_handler = std::make_shared<gateway::LaserScanHandler>(gateway.get());
  laser_handler->enable();
  laser_handler->enable_ros_publisher_mode(); // Enable ROS publisher mode
  laser_handler->disable_ros_subscriber_mode(); // Disable ROS subscriber mode
  gateway->register_handler(laser_handler);

  
  rclcpp::spin(gateway);
  rclcpp::shutdown();
  return 0;
}