#include "modular_gateway_sender/ros_gateway.hpp"
#include "std_msgs/msg/string.hpp"

namespace gateway {

class StringGateway : public RosGateway {
public:
  StringGateway() : RosGateway("string_gateway")
  {
    // Topic-specific parameter
    declare_parameter("ros_topic", "topic");
    std::string ros_topic = get_parameter("ros_topic").as_string();
    
    // Create ROS subscription
    subscription_ = create_subscription<std_msgs::msg::String>(
      ros_topic, 10,
      [this, ros_topic](std_msgs::msg::String::UniquePtr msg) {
        LOG_INFO(this->get_logger(), "Received from ROS: '%s'", msg->data.c_str());
        this->handle_string_message(ros_topic, msg->data);
      }
    );
  }

private:
  void handle_string_message(const std::string& topic, const std::string& message)
  {
    // Send the message with appropriate type
    send_message(topic, MessageType::STRING, message.data(), message.size());
  }

  // ROS subscription
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
};

} // namespace gateway

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<gateway::StringGateway>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}