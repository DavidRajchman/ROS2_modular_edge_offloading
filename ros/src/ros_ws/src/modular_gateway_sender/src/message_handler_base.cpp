#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/message_handler_base.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

MessageHandlerBase::MessageHandlerBase(RosGateway* gateway, const std::string& handler_name)
  : gateway_(gateway), handler_name_(handler_name), enabled_(false),
    ros_publisher_enabled_(true), ros_subscriber_enabled_(true)
{
}

bool MessageHandlerBase::send_message(const std::string& topic, MessageType type,
                                     const void* data, size_t size,
                                     const MessageOptions& options)
{
  // Only send message if handler is enabled AND subscriber mode is enabled
  // (subscriber mode = subscribing to ROS topics and sending to network)
  if (!is_ros_subscriber_enabled() || !gateway_) return false;
  return gateway_->send_message(topic, type, data, size, options);
}

} // namespace gateway