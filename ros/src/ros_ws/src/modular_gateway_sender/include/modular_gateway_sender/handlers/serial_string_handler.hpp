#ifndef SERIAL_STRING_HANDLER_HPP
#define SERIAL_STRING_HANDLER_HPP

#include "modular_gateway_sender/message_handler_base.hpp"
#include "std_msgs/msg/string.hpp"
#include "rclcpp/rclcpp.hpp"

#include <string>
#include <map>

namespace gateway {

class SerialStringHandler : public MessageHandlerBase {
public:
  SerialStringHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node);
  
  void initialize() override;
  void shutdown() override;
  
  bool can_process_message_type(MessageType type) const override;
  
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

#endif // SERIAL_STRING_HANDLER_HPP
