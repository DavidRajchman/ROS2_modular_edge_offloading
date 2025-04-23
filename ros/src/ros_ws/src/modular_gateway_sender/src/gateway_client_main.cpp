// gateway_main.cpp
#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

//handlers
#include "modular_gateway_sender/string_handler.hpp"
#include "modular_gateway_sender/laserscan_handler.hpp"



int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  
  // Create gateway in CLIENT mode
  auto gateway = std::make_shared<gateway::RosGateway>("gateway_client", gateway::TransportMode::CLIENT);
  
// Configure string handler as ROS subscriber only (subscribe to ROS, send to network)
auto string_handler = std::make_shared<gateway::StringHandler>(gateway.get());
string_handler->enable();                     // Enable the handler overall
string_handler->enable_ros_subscriber_mode(); // Enable ROS subscriber (already default)
string_handler->disable_ros_publisher_mode(); // Disable ROS publisher mode
gateway->register_handler(string_handler);

// Configure laser scan handler as ROS publisher only (receive from network, publish to ROS)
auto laser_handler = std::make_shared<gateway::LaserScanHandler>(gateway.get());
laser_handler->enable();                     // Enable the handler overall
laser_handler->enable_ros_subscriber_mode(); // Disable ROS subscriber mode
laser_handler->disable_ros_publisher_mode();   // Enable ROS publisher (already default)
gateway->register_handler(laser_handler);
  
  rclcpp::spin(gateway);
  rclcpp::shutdown();
  return 0;
}