// filepath: /home/ubuntu/ros_ws/src/modular_gateway_sender/src/message_handler_base.cpp
#include "modular_gateway_sender/message_handler_base.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

// Constructor: Initialize base handler with gateway and node references
// All handlers start disabled until explicitly enabled by controller
// Both publisher and subscriber modes enabled by default (overridden by configure_handler_mode)
MessageHandlerBase::MessageHandlerBase(RosGateway* gateway, rclcpp::Node::SharedPtr node, const std::string& handler_name)
  : gateway_(gateway), node_(node), handler_name_(handler_name), enabled_(false),
    ros_publisher_enabled_(true), ros_subscriber_enabled_(true)
{
}

// Send message to data plane via RosGateway binary framing
// Called by handler's ROS subscriber callback when local topic receives data
// Only sends if ros_subscriber_enabled (respects SUBSCRIBER_ONLY vs PUBLISHER_ONLY mode)
// Returns false if subscriber mode disabled or gateway not available
bool MessageHandlerBase::send_message(const std::string& topic, MessageType type,
                                     const void* data, size_t size,
                                     const MessageOptions& options)
{
  if (!is_ros_subscriber_enabled() || !gateway_) return false;
  return gateway_->send_message(topic, type, data, size, options);
}

// Configure handler mode based on offloading direction
// SUBSCRIBER_ONLY: VHC sends data to MEC (ROS subscriber active, publisher disabled)
// PUBLISHER_ONLY: MEC receives data from VHC (ROS publisher active, subscriber disabled)
// BOTH: Bidirectional communication (both subscriber and publisher active)
// Called by controller after session approval to set correct data flow direction
void MessageHandlerBase::configure_handler_mode(std::shared_ptr<MessageHandlerBase> handler, HandlerMode mode)
{
  if (!handler) return;
  
  handler->enable();  // Enable handler before configuring modes
  
  // Use a temporary logger as this is a static method.
  auto logger = rclcpp::get_logger("MessageHandlerBase");

  switch (mode) {
    case HandlerMode::SUBSCRIBER_ONLY:
      // VHC mode: Send local ROS topics to remote via data plane
      RCLCPP_INFO(logger, "Configuring handler '%s' as subscriber-only", 
               handler->get_name().c_str());
      handler->enable_ros_subscriber_mode();
      handler->disable_ros_publisher_mode();
      break;
    case HandlerMode::PUBLISHER_ONLY:
      // MEC mode: Receive remote data and publish to local ROS topics
      RCLCPP_INFO(logger, "Configuring handler '%s' as publisher-only", 
               handler->get_name().c_str());
      handler->disable_ros_subscriber_mode();
      handler->enable_ros_publisher_mode();
      break;
    case HandlerMode::BOTH:
      // Bidirectional: Both send and receive (unused in current VHC/MEC architecture)
      RCLCPP_INFO(logger, "Configuring handler '%s' with both modes", 
               handler->get_name().c_str());
      handler->enable_ros_subscriber_mode();
      handler->enable_ros_publisher_mode();
      break;
  }
}

} // namespace gateway