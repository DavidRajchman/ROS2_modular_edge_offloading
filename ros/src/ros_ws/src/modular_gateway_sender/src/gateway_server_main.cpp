#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

// Handlers
#include "modular_gateway_sender/handlers/string_handler.hpp"
#include "modular_gateway_sender/handlers/laserscan_handler.hpp"




int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  
  // Create gateway in CLIENT mode
  auto gateway = std::make_shared<gateway::RosGateway>("gateway_client", gateway::TransportMode::SERVER);
  
  // Configure string handler as ROS subscriber only (subscribe to ROS, send to network)
  auto string_handler = std::make_shared<gateway::StringHandler>(gateway.get());
  gateway::MessageHandlerBase::configure_handler_mode(string_handler, gateway::HandlerMode::PUBLISHER_ONLY);
  gateway->register_handler(string_handler);

  // Configure laser scan handler as ROS subscriber only (corrected from the original)
  auto laser_handler = std::make_shared<gateway::LaserScanHandler>(gateway.get());
  gateway::MessageHandlerBase::configure_handler_mode(laser_handler, gateway::HandlerMode::PUBLISHER_ONLY);
  gateway->register_handler(laser_handler);
  
  rclcpp::spin(gateway);
  rclcpp::shutdown();
  return 0;
}