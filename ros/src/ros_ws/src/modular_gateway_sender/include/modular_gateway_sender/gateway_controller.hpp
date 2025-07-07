#ifndef GATEWAY_CONTROLLER_HPP
#define GATEWAY_CONTROLLER_HPP

#include "rclcpp/rclcpp.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include "logging/logger.h"
#include <thread>
#include <atomic>

namespace gateway {

class GatewayController : public rclcpp::Node {
public:
  GatewayController(const rclcpp::NodeOptions& options);
  ~GatewayController();

private:
  void initialize_gateway();
  void connection_management_thread_func();
  void load_handlers();

  std::unique_ptr<TransportBase> transport_;
  std::unique_ptr<RosGateway> gateway_;
  
  std::thread connection_thread_;
  std::atomic<bool> running_{false};
  
  CppLogging::Logger logger_;
  
  // Parameters
  std::string transport_type_;
  std::string host_;
  int port_;
  std::vector<std::string> handlers_to_load_;
  int id_group_;
  int identifier_in_group_;
};

} // namespace gateway

#endif // GATEWAY_CONTROLLER_HPP