#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

// Handlers
#include "modular_gateway_sender/string_handler.hpp"
#include "modular_gateway_sender/laserscan_handler.hpp"

namespace gateway {

enum class HandlerMode {
  SUBSCRIBER_ONLY,  // Subscribe to ROS topics, send to network
  PUBLISHER_ONLY,   // Receive from network, publish to ROS topics
  BOTH              // Both modes enabled
};

// Configure handler operating mode
void configure_handler_mode(std::shared_ptr<MessageHandlerBase> handler, HandlerMode mode) {
  if (!handler) return;
  
  handler->enable(); // Always enable the handler
  
  switch (mode) {
    case HandlerMode::SUBSCRIBER_ONLY:
      LOG_INFO(rclcpp::get_logger("gateway"), "Configuring handler '%s' as subscriber-only", 
               handler->get_name().c_str());
      handler->enable_ros_subscriber_mode();
      handler->disable_ros_publisher_mode();
      break;
    case HandlerMode::PUBLISHER_ONLY:
      LOG_INFO(rclcpp::get_logger("gateway"), "Configuring handler '%s' as publisher-only", 
               handler->get_name().c_str());
      handler->disable_ros_subscriber_mode();
      handler->enable_ros_publisher_mode();
      break;
    case HandlerMode::BOTH:
      LOG_INFO(rclcpp::get_logger("gateway"), "Configuring handler '%s' with both modes", 
               handler->get_name().c_str());
      handler->enable_ros_subscriber_mode();
      handler->enable_ros_publisher_mode();
      break;
  }
}

} // namespace gateway

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  
  // Create gateway in CLIENT mode
  auto gateway = std::make_shared<gateway::RosGateway>("gateway_client", gateway::TransportMode::SERVER);
  
  // Configure string handler as ROS subscriber only (subscribe to ROS, send to network)
  auto string_handler = std::make_shared<gateway::StringHandler>(gateway.get());
  gateway::configure_handler_mode(string_handler, gateway::HandlerMode::PUBLISHER_ONLY);
  gateway->register_handler(string_handler);

  // Configure laser scan handler as ROS subscriber only (corrected from the original)
  auto laser_handler = std::make_shared<gateway::LaserScanHandler>(gateway.get());
  gateway::configure_handler_mode(laser_handler, gateway::HandlerMode::PUBLISHER_ONLY);
  gateway->register_handler(laser_handler);
  
  rclcpp::spin(gateway);
  rclcpp::shutdown();
  return 0;
}