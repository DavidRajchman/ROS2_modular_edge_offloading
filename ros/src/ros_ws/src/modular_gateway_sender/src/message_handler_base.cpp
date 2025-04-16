// message_handler_base.cpp
#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/message_handler_base.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

MessageHandlerBase::MessageHandlerBase(RosGateway* gateway, const std::string& handler_name)
  : gateway_(gateway), handler_name_(handler_name), enabled_(false)
{
}

bool MessageHandlerBase::send_message(const std::string& topic, MessageType type,
                                     const void* data, size_t size,
                                     const MessageOptions& options)
{
  if (!enabled_ || !gateway_) return false;
  return gateway_->send_message(topic, type, data, size, options);
}

} // namespace gateway