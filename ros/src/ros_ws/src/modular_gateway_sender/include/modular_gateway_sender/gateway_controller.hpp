#ifndef GATEWAY_CONTROLLER_HPP
#define GATEWAY_CONTROLLER_HPP

#include "rclcpp/rclcpp.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include "modular_gateway_sender/bridge_cp_client.hpp"
#include "modular_gateway_sender/discovery_client.hpp"
#include "modular_gateway_sender/handler_factory.hpp"
#include "modular_gateway_sender/srv/request_offloading.hpp"
#include "logging/logger.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace gateway {

// Represents an incoming request from a ROS service call
struct OffloadingRequestData {
    std::string task_id;
    // We can add the response promise here if we need to reply later
};

class GatewayController : public rclcpp::Node {
public:
  // The states of the controller's main logic thread
  enum class State {
    INITIALIZING,
    DISCOVERING,
    CONNECTING_TO_BRIDGE,
    WAITING_FOR_DP_CONNECTION,
    OPERATIONAL,
    FAILED
  };

  GatewayController(const rclcpp::NodeOptions& options);
  ~GatewayController();

private:
  // Main logic thread and state machine
  void control_thread_func();
  std::thread control_thread_;
  std::atomic<bool> running_{false};
  std::atomic<State> state_{State::INITIALIZING};
  
  // Thread synchronization
  std::mutex state_mutex_;
  std::condition_variable state_cv_;

  // Asynchronous request queue for ROS services
  std::queue<OffloadingRequestData> offloading_request_queue_;
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;

  // ROS 2 Service Handler (non-blocking)
  void offloading_request_service_handler(
    const std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Request> request,
    std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Response> response);

  // Data Plane Connection Management
  void data_plane_connection_thread_func();
  std::thread data_plane_connection_thread_;

  // Callbacks for async clients
  void on_discovery_success(const std::string& bridge_host, int bridge_port);
  void on_discovery_failure(const std::string& error_message);
  void on_dp_confirmed();
  void on_session_approved(const std::string& request_id);
  void on_session_denied(const std::string& request_id, const std::string& reason);

  // Core Components
  std::unique_ptr<RosGateway> gateway_;
  std::unique_ptr<DiscoveryClient> discovery_client_;
  std::unique_ptr<BridgeCpClient> bridge_cp_client_;
  std::unique_ptr<HandlerFactory> handler_factory_;
  
  // ROS 2 Members
  rclcpp::Service<modular_gateway_sender::srv::RequestOffloading>::SharedPtr offloading_service_;
  
  // Identity & Configuration
  std::string component_id_;
  std::string component_type_;
  std::string component_name_;
  int id_group_;
  int identifier_in_group_;
  int data_plane_listen_port_;
  std::string discovery_host_;
  int discovery_port_;

  // Discovered bridge connection info
  std::string bridge_cp_host_;
  int bridge_cp_port_;

  CppLogging::Logger logger_;
};

} // namespace gateway

#endif // GATEWAY_CONTROLLER_HPP