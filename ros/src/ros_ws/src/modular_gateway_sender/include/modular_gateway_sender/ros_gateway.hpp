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

  // Start the receiver thread
  bool start_receiver(bool wait_for_connection = false, int timeout_ms = 0);
      
  // Stop the receiver thread
  void stop_receiver();
  
private:
  void init_transport();

  // Message receiving thread function
  void receiver_thread_func();

  // Process a single message from the transport
  bool receive_and_process_message();
  
  // Parse binary header data into structured format
  bool parse_header(const std::vector<uint8_t>& header_data, 
                    std::string& topic, 
                    MessageType& type,
                    uint32_t& size,
                    MessageOptions& options);
                    
  
  
  std::unique_ptr<TransportBase> transport_;
  std::map<std::string, std::shared_ptr<MessageHandlerBase>> handlers_;

  // Thread management
  std::thread receiver_thread_;
  std::atomic<bool> receiver_running_{false};
  std::mutex transport_access_mutex_; // Protects access to transport during receive
};

} // namespace gateway

#endif // ROS_GATEWAY_HPP