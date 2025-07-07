#ifndef STRING_TEST_INPUT_HANDLER_HPP
#define STRING_TEST_INPUT_HANDLER_HPP

#include "modular_gateway_sender/message_handler_base.hpp"
#include "std_msgs/msg/string.hpp"
#include "logging/logger.h"
#include <map>

namespace gateway {

class StringTestInputHandler : public MessageHandlerBase {
public:
  StringTestInputHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node);
  
  void initialize() override;
  void shutdown() override;
  
  bool can_process_message_type(MessageType type) const override {
    return type == MessageType::STRING_TEST_INPUT;
  }
  
  bool process_and_publish_received_msg(
      const std::string& topic,
      MessageType type,
      const void* data,
      size_t size,
      const MessageOptions& options) override;
  
private:
  void handle_message(const std::string& topic, const std_msgs::msg::String::SharedPtr msg);
  
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
  std::string topic_name_;
  std::map<std::string, rclcpp::Publisher<std_msgs::msg::String>::SharedPtr> publishers_;
  CppLogging::Logger logger_;
};

} // namespace gateway

#endif // STRING_TEST_INPUT_HANDLER_HPP