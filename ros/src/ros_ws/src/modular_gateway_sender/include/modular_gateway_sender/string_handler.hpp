// string_handler.hpp
#ifndef STRING_HANDLER_HPP
#define STRING_HANDLER_HPP

#include "modular_gateway_sender/message_handler_base.hpp"
#include "std_msgs/msg/string.hpp"

namespace gateway {

class StringHandler : public MessageHandlerBase {
public:
  StringHandler(RosGateway* gateway);
  
  void initialize() override;
  void shutdown() override;
  
private:
  void handle_message(const std::string& topic, const std_msgs::msg::String::SharedPtr msg);
  
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
  std::string topic_name_;
};

} // namespace gateway

#endif // STRING_HANDLER_HPP