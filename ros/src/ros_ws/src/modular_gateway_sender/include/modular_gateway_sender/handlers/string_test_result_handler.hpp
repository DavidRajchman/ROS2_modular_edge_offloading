#ifndef STRING_TEST_RESULT_HANDLER_HPP
#define STRING_TEST_RESULT_HANDLER_HPP

#include "modular_gateway_sender/message_handler_base.hpp"
#include "std_msgs/msg/string.hpp"
#include <map>

namespace gateway {

class StringTestResultHandler : public MessageHandlerBase {
public:
  StringTestResultHandler(RosGateway* gateway);
  
  void initialize() override;
  void shutdown() override;
  
  bool can_process_message_type(MessageType type) const override {
    return type == MessageType::STRING_TEST_RESULT;
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
};

} // namespace gateway

#endif // STRING_TEST_RESULT_HANDLER_HPP