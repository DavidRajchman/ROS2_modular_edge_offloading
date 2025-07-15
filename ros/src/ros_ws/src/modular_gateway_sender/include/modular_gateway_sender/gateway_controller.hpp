#ifndef GATEWAY_CONTROLLER_HPP
#define GATEWAY_CONTROLLER_HPP

#include "rclcpp/rclcpp.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include "modular_gateway_sender/bridge_cp_client.hpp"
#include "modular_gateway_sender/discovery_client.hpp"
#include "modular_gateway_sender/handler_factory.hpp"

#include "logging/logger.h"

// Include the auto-generated service headers
#include "modular_gateway_sender/srv/request_offloading.hpp"
#include "modular_gateway_sender/srv/terminate_offloading.hpp"

#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <map>
#include <vector>
#include <chrono>

namespace gateway {

// Represents an incoming request from a ROS service call
struct OffloadingRequestData {
    std::string task_id;
    // We can add the response promise here if we need to reply later
};

// Represents the details of a specific offloading task
struct TaskDetails {
    std::string task_name;
    std::vector<std::string> required_handlers; // e.g., {"std_msgs/msg/String", "sensor_msgs/msg/LaserScan"}
};

// Represents the state of a single offloading session
struct SessionState {
    std::string request_id;
    std::string task_id;
    std::chrono::steady_clock::time_point last_keepalive_sent;
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

  /**
   * @brief Initialize components that require shared_from_this().
   * 
   * This method must be called after the constructor completes and the object
   * is managed by a shared_ptr. It creates the RosGateway and HandlerFactory
   * instances that need access to shared_from_this().
   */
  void initialize();

private:
  // Main logic thread and state machine
  void control_thread_func();
  std::thread control_thread_;
  std::atomic<bool> running_{false};
  std::atomic<State> state_{State::INITIALIZING};
  
  // Thread synchronization
  std::mutex state_mutex_;
  std::condition_variable state_cv_;

  // Asynchronous request queues for ROS services
  std::queue<OffloadingRequestData> offloading_request_queue_;
  std::queue<std::string> termination_request_queue_; // Holds request_ids to terminate
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;

  // ROS 2 Service Handlers (non-blocking)
  void offloading_request_service_handler(
    const std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Request> request,
    std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Response> response);
  
  void terminate_offloading_service_handler(
    const std::shared_ptr<modular_gateway_sender::srv::TerminateOffloading::Request> request,
    std::shared_ptr<modular_gateway_sender::srv::TerminateOffloading::Response> response);

  // Data Plane Connection Management
  void data_plane_connection_thread_func();
  std::thread data_plane_connection_thread_;

  // Session Teardown Logic
  void handle_session_teardown(const std::string& request_id);

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
  rclcpp::Service<modular_gateway_sender::srv::TerminateOffloading>::SharedPtr terminate_offloading_service_;
  
  // Session and Task Management
  std::atomic<uint64_t> request_id_counter_{1};
  std::map<std::string, TaskDetails> task_database_;
  std::map<std::string, SessionState> active_sessions_; // Maps request_id to session state
  std::mutex session_mutex_;

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