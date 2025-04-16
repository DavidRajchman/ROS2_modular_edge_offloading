// ros_gateway.hpp
#ifndef ROS_GATEWAY_HPP
#define ROS_GATEWAY_HPP

#include <memory>
#include <map>
#include "rclcpp/rclcpp.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include "modular_gateway_sender/message_header.hpp"
#include "modular_gateway_sender/message_handler_base.hpp"

namespace gateway {

class RosGateway : public rclcpp::Node {
public:
  RosGateway(const std::string& node_name);
  virtual ~RosGateway();
  
  // Handler management
  bool register_handler(std::shared_ptr<MessageHandlerBase> handler);
  bool unregister_handler(const std::string& handler_name);
  void enable_handler(const std::string& handler_name);
  void disable_handler(const std::string& handler_name);
  
  // Message sending (used by handlers)
  bool send_message(const std::string& topic, MessageType type,
                    const void* data, size_t size,
                    const MessageOptions& options);
  
private:
  void init_transport();
  
  std::unique_ptr<TransportBase> transport_;
  std::map<std::string, std::shared_ptr<MessageHandlerBase>> handlers_;
};

} // namespace gateway

#endif // ROS_GATEWAY_HPP