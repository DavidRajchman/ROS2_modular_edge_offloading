#ifndef JOINT_STATE_HANDLER_HPP
#define JOINT_STATE_HANDLER_HPP

#include "modular_gateway_sender/message_handler_base.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "rclcpp/rclcpp.hpp"

#include <string>
#include <map>

namespace gateway {

class JointStateHandler : public MessageHandlerBase {
public:
  JointStateHandler(RosGateway* gateway, rclcpp::Node::SharedPtr node);
  
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
  void handle_message(const std::string& topic, const sensor_msgs::msg::JointState::SharedPtr msg);
  
  rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr subscription_;
  std::string topic_name_;
  std::map<std::string, rclcpp::Publisher<sensor_msgs::msg::JointState>::SharedPtr> publishers_;
  CppLogging::Logger logger_;
};

} // namespace gateway

#endif // JOINT_STATE_HANDLER_HPP
