#ifndef ROS_GATEWAY_HPP
#define ROS_GATEWAY_HPP

#include "modular_gateway_sender/transport_base.hpp"
#include "modular_gateway_sender/message_handler_base.hpp"
#include "logging/logger.h"
#include "rclcpp/rclcpp.hpp"

#include <string>
#include <vector>
#include <memory>
#include <map>


namespace gateway {

class RosGateway {
public:
  RosGateway(
    rclcpp::Node::SharedPtr node,
    std::unique_ptr<TransportBase> transport
  );
  ~RosGateway();
  
  bool send_message(const std::string& topic, MessageType type,
                   const void* data, size_t size,
                   const MessageOptions& options);
  
  bool register_handler(std::shared_ptr<MessageHandlerBase> handler);
  bool unregister_handler(const std::string& handler_name);
  void enable_handler(const std::string& handler_name);
  void disable_handler(const std::string& handler_name);
  
  void start_receiver();
  void stop_receiver();

  void set_identity(uint8_t id_group, uint8_t identifier_in_group);
  
  TransportBase* get_transport();

private:
  rclcpp::Node::SharedPtr node_;
  uint8_t id_group_ = 0;
  uint8_t identifier_in_group_ = 0;

  std::map<std::string, std::shared_ptr<MessageHandlerBase>> handlers_;
  
  std::unique_ptr<TransportBase> transport_;
  std::vector<uint8_t> header_buffer_;
  std::vector<char> topic_buffer_;
  std::vector<uint8_t> data_buffer_;

  std::thread receiver_thread_;
  std::atomic<bool> receiver_running_{false};
  
  CppLogging::Logger logger_;
  
  void receiver_thread_func();
  bool receive_and_process_message();
};

} // namespace gateway

#endif // ROS_GATEWAY_HPP