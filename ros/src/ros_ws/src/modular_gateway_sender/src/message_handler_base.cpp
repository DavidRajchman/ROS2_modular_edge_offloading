// filepath: /home/ubuntu/ros_ws/src/modular_gateway_sender/src/message_handler_base.cpp
#include "modular_gateway_sender/message_handler_base.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

MessageHandlerBase::MessageHandlerBase(RosGateway* gateway, rclcpp::Node::SharedPtr node, const std::string& handler_name)
  : gateway_(gateway), node_(node), handler_name_(handler_name), enabled_(false),
    ros_publisher_enabled_(true), ros_subscriber_enabled_(true)
{
}

bool MessageHandlerBase::send_message(const std::string& topic, MessageType type,
                                     const void* data, size_t size,
                                     const MessageOptions& options)
{
  if (!is_ros_subscriber_enabled() || !gateway_) return false;
  return gateway_->send_message(topic, type, data, size, options);
}

void MessageHandlerBase::configure_handler_mode(std::shared_ptr<MessageHandlerBase> handler, HandlerMode mode)
{
  if (!handler) return;
  
  handler->enable();
  
  // Use a temporary logger as this is a static method.
  auto logger = rclcpp::get_logger("MessageHandlerBase");

  switch (mode) {
    case HandlerMode::SUBSCRIBER_ONLY:
      RCLCPP_INFO(logger, "Configuring handler '%s' as subscriber-only", 
               handler->get_name().c_str());
      handler->enable_ros_subscriber_mode();
      handler->disable_ros_publisher_mode();
      break;
    case HandlerMode::PUBLISHER_ONLY:
      RCLCPP_INFO(logger, "Configuring handler '%s' as publisher-only", 
               handler->get_name().c_str());
      handler->disable_ros_subscriber_mode();
      handler->enable_ros_publisher_mode();
      break;
    case HandlerMode::BOTH:
      RCLCPP_INFO(logger, "Configuring handler '%s' with both modes", 
               handler->get_name().c_str());
      handler->enable_ros_subscriber_mode();
      handler->enable_ros_publisher_mode();
      break;
  }
}

} // namespace gateway