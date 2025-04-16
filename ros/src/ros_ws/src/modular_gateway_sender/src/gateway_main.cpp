// gateway_main.cpp
#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"
#include "modular_gateway_sender/string_handler.hpp"

// Include other handlers...

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  
  auto gateway = std::make_shared<gateway::RosGateway>("modular_gateway");
  
  // Register handlers
  gateway->register_handler(std::make_shared<gateway::StringHandler>(gateway.get()));
  
  // Register more handlers
  // gateway->register_handler(...);
  
  rclcpp::spin(gateway);
  rclcpp::shutdown();
  return 0;
}