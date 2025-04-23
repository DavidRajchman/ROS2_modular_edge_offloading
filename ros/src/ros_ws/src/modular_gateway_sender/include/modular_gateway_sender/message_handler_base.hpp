#ifndef MESSAGE_HANDLER_BASE_HPP
#define MESSAGE_HANDLER_BASE_HPP

#include <string>
#include "rclcpp/rclcpp.hpp"
#include "modular_gateway_sender/message_header.hpp"

namespace gateway {

class RosGateway; // Forward declaration

class MessageHandlerBase {
public:
  MessageHandlerBase(RosGateway* gateway, const std::string& handler_name);
  virtual ~MessageHandlerBase() = default;
  
  virtual void initialize() = 0;
  virtual void shutdown() = 0;
  
  const std::string& get_name() const { return handler_name_; }
  bool is_enabled() const { return enabled_; }
  void enable() { enabled_ = true; }
  void disable() { enabled_ = false; }
  
  // ROS Publisher/Subscriber mode control
  void enable_ros_publisher_mode() { ros_publisher_enabled_ = true; }
  void disable_ros_publisher_mode() { ros_publisher_enabled_ = false; }
  void enable_ros_subscriber_mode() { ros_subscriber_enabled_ = true; }
  void disable_ros_subscriber_mode() { ros_subscriber_enabled_ = false; }
  
  bool is_ros_publisher_enabled() const { return enabled_ && ros_publisher_enabled_; }
  bool is_ros_subscriber_enabled() const { return enabled_ && ros_subscriber_enabled_; }
  
  // New method to check if handler can process a message type
  virtual bool can_process_message_type(MessageType /* type */) const {
    return false; // Default implementation processes no message types
  }
  
  // New method to process and publish received messages to ROS
  virtual bool process_and_publish_received_msg(
      const std::string& /* topic */,
      MessageType /* type */,
      const void* /* data */,
      size_t /* size */,
      const MessageOptions& /* options */) {
    return false; // Default implementation does no processing
  }
  
protected:
  RosGateway* gateway_; // Non-owning pointer to parent gateway
  std::string handler_name_;
  bool enabled_ = false;
  bool ros_publisher_enabled_ = true;  // Default: can publish to ROS topics (receive from network)
  bool ros_subscriber_enabled_ = true; // Default: can subscribe to ROS topics (send to network)
  
  // Helper methods for handlers to send messages through the gateway
  bool send_message(const std::string& topic, MessageType type,
                    const void* data, size_t size,
                    const MessageOptions& options);
};

} // namespace gateway

#endif // MESSAGE_HANDLER_BASE_HPP