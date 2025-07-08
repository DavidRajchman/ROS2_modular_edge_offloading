#ifndef MESSAGE_HANDLER_BASE_HPP
#define MESSAGE_HANDLER_BASE_HPP

#include "logging/logger.h"
#include "rclcpp/rclcpp.hpp"
#include <string>
#include <vector>
#include <memory>

namespace gateway {

enum class HandlerMode {
  SUBSCRIBER_ONLY,
  PUBLISHER_ONLY,
  BOTH
};

class RosGateway; // Forward declaration

class MessageHandlerBase {
public:
  MessageHandlerBase(RosGateway* gateway, rclcpp::Node::SharedPtr node, const std::string& handler_name);
  virtual ~MessageHandlerBase() = default;
  
  virtual void initialize() = 0;
  virtual void shutdown() = 0;
  
  const std::string& get_name() const { return handler_name_; }
  bool is_enabled() const { return enabled_; }
  void enable() { enabled_ = true; }
  void disable() { enabled_ = false; }
  
  void enable_ros_publisher_mode() { ros_publisher_enabled_ = true; }
  void disable_ros_publisher_mode() { ros_publisher_enabled_ = false; }
  void enable_ros_subscriber_mode() { ros_subscriber_enabled_ = true; }
  void disable_ros_subscriber_mode() { ros_subscriber_enabled_ = false; }
  
  bool is_ros_publisher_enabled() const { return enabled_ && ros_publisher_enabled_; }
  bool is_ros_subscriber_enabled() const { return enabled_ && ros_subscriber_enabled_; }
  
  static void configure_handler_mode(std::shared_ptr<MessageHandlerBase> handler, HandlerMode mode);

  virtual bool can_process_message_type(MessageType /* type */) const {
    return false;
  }
  
  virtual bool process_and_publish_received_msg(
      const std::string& /* topic */,
      MessageType /* type */,
      const void* /* data */,
      size_t /* size */,
      const MessageOptions& /* options */) {
    return false;
  }
  
protected:
  RosGateway* gateway_;
  rclcpp::Node::SharedPtr node_;
  std::string handler_name_;
  bool enabled_ = false;
  bool ros_publisher_enabled_ = true;
  bool ros_subscriber_enabled_ = true;
  
  bool send_message(const std::string& topic, MessageType type,
                    const void* data, size_t size,
                    const MessageOptions& options);
};

} // namespace gateway

#endif // MESSAGE_HANDLER_BASE_HPP