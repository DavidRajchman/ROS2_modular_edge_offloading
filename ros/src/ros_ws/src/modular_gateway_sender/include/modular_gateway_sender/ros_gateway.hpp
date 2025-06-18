#ifndef ROS_GATEWAY_HPP
#define ROS_GATEWAY_HPP

#include <memory>
#include <map>
#include <mutex>
#include <thread>
#include "rclcpp/rclcpp.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include "modular_gateway_sender/message_header.hpp"
#include "modular_gateway_sender/message_handler_base.hpp"

#include "logging/logger.h"
#include "logging/config.h"

namespace gateway {

enum class TransportMode {
  CLIENT,
  SERVER
};

class RosGateway : public rclcpp::Node {
public:
  RosGateway(
    const std::string& node_name, 
    TransportMode transport_mode = TransportMode::CLIENT, 
    const rclcpp::NodeOptions& options = rclcpp::NodeOptions()
  );
  ~RosGateway();
  
  // Existing methods
  bool send_message(const std::string& topic, MessageType type,
                   const void* data, size_t size,
                   const MessageOptions& options);
  
  bool register_handler(std::shared_ptr<MessageHandlerBase> handler);
  bool unregister_handler(const std::string& handler_name);
  void enable_handler(const std::string& handler_name);
  void disable_handler(const std::string& handler_name);
  
  bool start_receiver(bool wait_for_connection = true, int timeout_ms = 5000);
  void stop_receiver();

  void set_gateway_id(uint8_t id_group, uint8_t identifier_in_group);
  
private:
  TransportMode transport_mode_;

  uint8_t id_group_;
  uint8_t identifier_in_group_;

  std::map<std::string, std::shared_ptr<MessageHandlerBase>> handlers_;
  
  std::unique_ptr<TransportBase> transport_;
  std::vector<uint8_t> header_buffer_;
  std::vector<char> topic_buffer_;
  std::vector<uint8_t> data_buffer_;

  std::thread receiver_thread_;
  std::atomic<bool> receiver_running_{false};
  
  CppLogging::Logger logger_;
  
  void init_transport();
  void receiver_thread_func();
  bool receive_and_process_message();
};

} // namespace gateway

#endif // ROS_GATEWAY_HPP