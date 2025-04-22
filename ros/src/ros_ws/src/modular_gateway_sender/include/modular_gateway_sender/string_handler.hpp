#ifndef STRING_HANDLER_HPP
#define STRING_HANDLER_HPP

#include "modular_gateway_sender/message_handler_base.hpp"
#include "std_msgs/msg/string.hpp"
#include <map>

namespace gateway {

class StringHandler : public MessageHandlerBase {
public:
  StringHandler(RosGateway* gateway);
  
  void initialize() override;
  void shutdown() override;
  
  // Add these methods for message receiving
  bool can_process_message_type(MessageType type) const override {
    return type == MessageType::STRING;
  }
  
  bool process_and_publish_received_msg(
      const std::string& topic,
      MessageType type,
      const void* data,
      size_t size,
      const MessageOptions& options) override;
  
private:
  // Existing method for sending messages
  void handle_message(const std::string& topic, const std_msgs::msg::String::SharedPtr msg);
  
  // Existing members
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
  std::string topic_name_;
  
  // New members for receiving messages
  std::map<std::string, rclcpp::Publisher<std_msgs::msg::String>::SharedPtr> publishers_;
};

} // namespace gateway

#endif // STRING_HANDLER_HPP