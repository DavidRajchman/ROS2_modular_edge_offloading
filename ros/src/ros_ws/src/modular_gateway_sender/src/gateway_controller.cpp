#include "modular_gateway_sender/gateway_controller.hpp"
#include "modular_gateway_sender/discovery_client.hpp"
#include "discovery_protocol/protocol.hpp"
#include "modular_gateway_sender/handler_factory.hpp"
#include "modular_gateway_sender/transport_base.hpp"



namespace gateway {

GatewayController::GatewayController(const rclcpp::NodeOptions& options)
  : rclcpp::Node("gateway_controller", options),
    logger_(CppLogging::Logger("gateway"))
{
  logger_.Info("gateway_controller.cpp: Constructing GatewayController...");

  // Declare and load all required parameters
  this->declare_parameter<std::string>("discovery_service.host", "127.0.0.1");
  this->declare_parameter<int>("discovery_service.port", 19900);
  this->declare_parameter<std::string>("identity.component_type", "VHC");
  this->declare_parameter<std::string>("identity.component_name", "default_vhc");
  this->declare_parameter<int>("identity.group_id", 1);
  this->declare_parameter<int>("identity.id_in_group", 1);
  this->declare_parameter<int>("data_plane.listen_port", 7401);

  discovery_host_ = this->get_parameter("discovery_service.host").as_string();
  discovery_port_ = this->get_parameter("discovery_service.port").as_int();
  component_type_ = this->get_parameter("identity.component_type").as_string();
  component_name_ = this->get_parameter("identity.component_name").as_string();
  id_group_ = this->get_parameter("identity.group_id").as_int();
  identifier_in_group_ = this->get_parameter("identity.id_in_group").as_int();
  data_plane_listen_port_ = this->get_parameter("data_plane.listen_port").as_int();
  component_id_ = std::to_string(id_group_) + ":" + std::to_string(identifier_in_group_);

  // --- Mock Task Database Initialization ---
  task_database_["NAV_BASIC"] = {"Navigation Basic", {"sensor_msgs/msg/LaserScan"}};
  task_database_["TELEOP_FULL"] = {"Teleoperation Full", {"std_msgs/msg/String", "sensor_msgs/msg/LaserScan"}};
  task_database_["TEST_INPUT_ONLY"] = {"Test Input Only", {"test/string_input"}};
  task_database_["TEST_RESULT_ONLY"] = {"Test Result Only", {"test/string_result"}};
  logger_.Info("gateway_controller.cpp: Initialized mock task database with {} tasks.", task_database_.size());
  // -----------------------------------------

  // Create the data plane transport and the RosGateway instance
  auto data_plane_transport = std::make_unique<TcpServerTransport>(data_plane_listen_port_, 1);
  gateway_ = std::make_unique<RosGateway>(this->shared_from_this(), std::move(data_plane_transport));
  gateway_->set_identity(static_cast<uint8_t>(id_group_), static_cast<uint8_t>(identifier_in_group_));

  // Create the handler factory
  handler_factory_ = std::make_unique<HandlerFactory>(gateway_.get(), this->shared_from_this());

  // Create the ROS 2 services
  offloading_service_ = this->create_service<modular_gateway_sender::srv::RequestOffloading>(
    "request_offloading",
    std::bind(&GatewayController::offloading_request_service_handler, this, std::placeholders::_1, std::placeholders::_2)
  );

  terminate_offloading_service_ = this->create_service<modular_gateway_sender::srv::TerminateOffloading>(
    "terminate_offloading",
    std::bind(&GatewayController::terminate_offloading_service_handler, this, std::placeholders::_1, std::placeholders::_2)
  );

  // Start the main control logic thread
  running_ = true;
  control_thread_ = std::thread(&GatewayController::control_thread_func, this);
  
  logger_.Info("gateway_controller.cpp: GatewayController constructed successfully. Control thread started.");
}

GatewayController::~GatewayController()
{
  logger_.Info("gateway_controller.cpp: Shutting down GatewayController...");
  running_ = false;
  
  // Notify all condition variables to unblock threads
  state_cv_.notify_all();
  queue_cv_.notify_all();

  if (discovery_client_) {
    discovery_client_->stop();
  }
  if (bridge_cp_client_) {
    bridge_cp_client_->stop();
  }

  if (control_thread_.joinable()) {
    control_thread_.join();
  }
  if (data_plane_connection_thread_.joinable()) {
    data_plane_connection_thread_.join();
  }
  if (gateway_) {
    gateway_->stop_receiver();
  }
  
  logger_.Info("gateway_controller.cpp: GatewayController shut down.");
}

void GatewayController::offloading_request_service_handler(
  const std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Request> request,
  std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Response> response)
{
    if (state_.load() != State::OPERATIONAL) {
        logger_.Warn("gateway_controller.cpp: Offloading request for task '{}' received, but controller is not operational. Rejecting.", request->task_id);
        response->approved = false;
        response->message = "Gateway is not in OPERATIONAL state.";
        return;
    }

    // Check if the requested task exists in our database
    if (task_database_.find(request->task_id) == task_database_.end()) {
        logger_.Error("gateway_controller.cpp: Offloading request for unknown task_id '{}'. Rejecting.", request->task_id);
        response->approved = false;
        response->message = "Unknown task_id: " + request->task_id;
        return;
    }

    logger_.Info("gateway_controller.cpp: Queuing offloading request for task_id: {}", request->task_id);
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        offloading_request_queue_.push({request->task_id});
    }
    queue_cv_.notify_one();

    // For now, we reply immediately. In a real system, we might wait for a future/promise.
    response->approved = true;
    response->message = "Request queued for processing by the bridge.";
}

void GatewayController::terminate_offloading_service_handler(
  const std::shared_ptr<modular_gateway_sender::srv::TerminateOffloading::Request> request,
  std::shared_ptr<modular_gateway_sender::srv::TerminateOffloading::Response> response)
{
    if (state_.load() != State::OPERATIONAL) {
        logger_.Warn("gateway_controller.cpp: Terminate request for session '{}' received, but controller is not operational. Rejecting.", request->request_id);
        response->success = false;
        response->message = "Gateway is not in OPERATIONAL state.";
        return;
    }

    logger_.Info("gateway_controller.cpp: Queuing termination request for session_id: {}", request->request_id);
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        termination_request_queue_.push(request->request_id);
    }
    queue_cv_.notify_one();

    response->success = true;
    response->message = "Termination request queued.";
}

void GatewayController::control_thread_func() {
    // Define keepalive interval
    const auto keepalive_interval = std::chrono::seconds(15);

    while(running_) {
        State current_state = state_.load();
        logger_.Info("gateway_controller.cpp: Control thread processing state: {}", static_cast<int>(current_state));

        switch(current_state) {
            case State::INITIALIZING:
            {
                // Start the data plane server listening for connections
                auto transport = dynamic_cast<TcpServerTransport*>(gateway_->get_transport());
                if (transport && transport->connect()) {
                    data_plane_connection_thread_ = std::thread(&GatewayController::data_plane_connection_thread_func, this);
                } else {
                    logger_.Error("gateway_controller.cpp: CRITICAL: Failed to start Data Plane server. Entering FAILED state.");
                    state_ = State::FAILED;
                    break;
                }
                
                // Transition to the discovery state
                state_ = State::DISCOVERING;
                break;
            }
            case State::DISCOVERING:
            {
                logger_.Info("gateway_controller.cpp: Starting discovery process...");
                discovery_client_ = std::make_unique<DiscoveryClient>();
                
                auto component_type_enum = discovery_protocol::string_to_component_type(component_type_);

                discovery_client_->start(
                    discovery_host_,
                    discovery_port_,
                    component_type_enum,
                    component_name_,
                    std::bind(&GatewayController::on_discovery_success, this, std::placeholders::_1, std::placeholders::_2),
                    std::bind(&GatewayController::on_discovery_failure, this, std::placeholders::_1)
                );

                // Wait for discovery to complete (or fail)
                std::unique_lock<std::mutex> lock(state_mutex_);
                state_cv_.wait(lock, [this]{ return state_ != State::DISCOVERING || !running_; });
                break;
            }
            case State::CONNECTING_TO_BRIDGE:
            {
                bridge_cp_client_ = std::make_unique<BridgeCpClient>();
                
                // Define callbacks for the bridge client
                auto dp_confirmed_cb = std::bind(&GatewayController::on_dp_confirmed, this);
                auto session_approved_cb = std::bind(&GatewayController::on_session_approved, this, std::placeholders::_1);
                auto session_denied_cb = std::bind(&GatewayController::on_session_denied, this, std::placeholders::_1, std::placeholders::_2);

                if (!bridge_cp_client_->start(bridge_cp_host_, bridge_cp_port_, dp_confirmed_cb, session_approved_cb, session_denied_cb)) {
                    logger_.Error("gateway_controller.cpp: CRITICAL: Could not connect to Bridge Control Plane. Retrying in 5s...");
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    // Stay in this state to retry on the next loop iteration
                    break; 
                }
                
                logger_.Info("gateway_controller.cpp: Bridge Control Plane client connected. Sending DP Info...");
                std::string my_ip = "127.0.0.1"; // TODO: Get this dynamically
                bridge_cp_client_->send_dp_info(component_id_, my_ip, data_plane_listen_port_);
                
                logger_.Info("gateway_controller.cpp: DP Info sent. Waiting for DP confirmation from Bridge.");
                state_ = State::WAITING_FOR_DP_CONNECTION;
                break;
            }
            case State::WAITING_FOR_DP_CONNECTION:
            {
                // The on_dp_confirmed() callback will change the state.
                // We wait here until that happens.
                logger_.Info("gateway_controller.cpp: Waiting for Data Plane connection to be confirmed by Bridge...");
                std::unique_lock<std::mutex> lock(state_mutex_);
                state_cv_.wait(lock, [this]{ return state_ != State::WAITING_FOR_DP_CONNECTION || !running_; });
                break;
            }
            case State::OPERATIONAL:
            {
                // In operational state, we process queues and send keepalives.
                // Wait for a new request or for the keepalive timeout.
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait_for(lock, std::chrono::seconds(5), [this]{ 
                    return !offloading_request_queue_.empty() || !termination_request_queue_.empty() || !running_; 
                });

                if (!running_) break;

                // 1. Process termination requests
                while (!termination_request_queue_.empty()) {
                    std::string request_id_to_terminate = termination_request_queue_.front();
                    termination_request_queue_.pop();
                    lock.unlock(); // Unlock while handling
                    
                    logger_.Info("gateway_controller.cpp: Dequeued termination request for session '{}'", request_id_to_terminate);
                    bridge_cp_client_->send_session_terminate_request(component_id_, request_id_to_terminate);
                    handle_session_teardown(request_id_to_terminate);

                    lock.lock(); // Re-lock to check queue condition
                }

                // 2. Process new offloading requests
                while (!offloading_request_queue_.empty()) {
                    OffloadingRequestData request = offloading_request_queue_.front();
                    offloading_request_queue_.pop();
                    lock.unlock(); // Unlock while handling

                    logger_.Info("gateway_controller.cpp: Dequeued and processing offloading request for task '{}'", request.task_id);
                    
                    auto db_it = task_database_.find(request.task_id);
                    if (db_it == task_database_.end()) {
                        logger_.Error("gateway_controller.cpp: Dequeued task '{}' but it's not in the database. This should not happen.", request.task_id);
                    } else {
                        std::string task_name = db_it->second.task_name;
                        std::string request_id = component_id_ + ":" + std::to_string(request_id_counter_++);

                        {
                            std::lock_guard<std::mutex> session_lock(session_mutex_);
                            active_sessions_[request_id] = {request_id, request.task_id, std::chrono::steady_clock::now()};
                        }

                        bridge_cp_client_->send_offload_request(component_id_, request_id, request.task_id, task_name);
                    }
                    lock.lock(); // Re-lock to check queue condition
                }
                lock.unlock();

                // 3. Send keepalives for active sessions
                std::lock_guard<std::mutex> session_lock(session_mutex_);
                auto now = std::chrono::steady_clock::now();
                for (auto& pair : active_sessions_) {
                    if (now - pair.second.last_keepalive_sent > keepalive_interval) {
                        logger_.Info("gateway_controller.cpp: Sending keepalive for session '{}'", pair.first);
                        bridge_cp_client_->send_session_keepalive(component_id_, pair.first);
                        pair.second.last_keepalive_sent = now;
                    }
                }
                break;
            }
            case State::FAILED:
            {
                logger_.Error("gateway_controller.cpp: Controller has entered FAILED state. Halting operations.");
                running_ = false; // Stop the loop
                break;
            }
        }
    }
    logger_.Info("gateway_controller.cpp: Control thread exited.");
}

void GatewayController::data_plane_connection_thread_func()
{
  TransportBase* transport = gateway_->get_transport();
  if (!transport) {
      logger_.Error("gateway_controller.cpp: Transport is null in data plane thread. Shutting down.");
      state_ = State::FAILED;
      state_cv_.notify_one();
      return;
  }

  while (running_) {
    if (!transport->is_connected()) {
      if (gateway_->is_receiver_running()) {
        logger_.Warn("gateway_controller.cpp: Data plane disconnected. Stopping receiver.");
        gateway_->stop_receiver();
      }
      logger_.Info("gateway_controller.cpp: Data plane server listening. Waiting for Bridge DP to connect...");
      
      if (transport->accept_connection()) {
        logger_.Info("gateway_controller.cpp: Data plane connection ACCEPTED from Bridge DP.");
        // The connection is accepted, but we wait for the Bridge to *confirm* it via the control plane.
        // The on_dp_confirmed() callback is the trigger to start the receiver.
      } else {
          if (running_) {
            logger_.Warn("gateway_controller.cpp: Failed to accept data plane connection. Retrying in 2s...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
          }
      }
    } else {
        // If we are connected, just sleep. The receiver is managed by state transitions.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

void GatewayController::handle_session_teardown(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    auto session_it = active_sessions_.find(request_id);
    if (session_it == active_sessions_.end()) {
        logger_.Warn("gateway_controller.cpp: Teardown requested for unknown or already terminated session '{}'.", request_id);
        return;
    }

    const std::string& task_id = session_it->second.task_id;
    auto task_it = task_database_.find(task_id);
    if (task_it != task_database_.end()) {
        const auto& required_handlers = task_it->second.required_handlers;
        logger_.Info("gateway_controller.cpp: Tearing down session '{}'. Unregistering {} handlers for task '{}'.", request_id, required_handlers.size(), task_id);
        for (const auto& handler_type : required_handlers) {
            // The handler name is the same as its type string in our current factory implementation
            gateway_->unregister_handler(handler_type);
        }
    } else {
        logger_.Error("gateway_controller.cpp: Could not find task details for task_id '{}' during teardown of session '{}'.", task_id, request_id);
    }

    active_sessions_.erase(session_it);
    logger_.Info("gateway_controller.cpp: Session '{}' removed.", request_id);
}

// --- Callback Implementations ---

void GatewayController::on_discovery_success(const std::string& bridge_host, int bridge_port) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ == State::DISCOVERING) {
        logger_.Info("gateway_controller.cpp: Discovery successful. Bridge CP at {}:{}", bridge_host, bridge_port);
        bridge_cp_host_ = bridge_host;
        bridge_cp_port_ = bridge_port;
        state_ = State::CONNECTING_TO_BRIDGE;
    }
    state_cv_.notify_one();
}

void GatewayController::on_discovery_failure(const std::string& error_message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    logger_.Error("gateway_controller.cpp: Discovery failed: {}. Retrying in 10s.", error_message);
    // Instead of failing, we will retry discovery after a delay
    std::this_thread::sleep_for(std::chrono::seconds(10));
    if (state_ == State::DISCOVERING) {
        // The loop in control_thread_func will re-initiate discovery
    }
    state_cv_.notify_one(); // Wake up the control thread to retry
}

void GatewayController::on_dp_confirmed() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ == State::WAITING_FOR_DP_CONNECTION) {
        logger_.Info("gateway_controller.cpp: DP connection confirmed by Bridge. Starting receiver and transitioning to OPERATIONAL.");
        gateway_->start_receiver();
        state_ = State::OPERATIONAL;
    } else {
        logger_.Warn("gateway_controller.cpp: Received DP confirmation in unexpected state: {}", static_cast<int>(state_.load()));
    }
    state_cv_.notify_one();
}

void GatewayController::on_session_approved(const std::string& request_id) {
    logger_.Info("gateway_controller.cpp: Session with request_id '{}' approved by Bridge.", request_id);
    
    std::lock_guard<std::mutex> lock(session_mutex_);
    auto session_it = active_sessions_.find(request_id);
    if (session_it == active_sessions_.end()) {
        logger_.Error("gateway_controller.cpp: Received approval for unknown or already handled request_id '{}'.", request_id);
        return;
    }

    // Update the keepalive timestamp to start the timer now that it's approved
    session_it->second.last_keepalive_sent = std::chrono::steady_clock::now();

    const std::string& task_id = session_it->second.task_id;
    auto task_it = task_database_.find(task_id);
    if (task_it == task_database_.end()) {
        logger_.Error("gateway_controller.cpp: Could not find task details for task_id '{}' from approved session '{}'.", task_id, request_id);
        return;
    }

    const auto& required_handlers = task_it->second.required_handlers;
    logger_.Info("gateway_controller.cpp: Activating {} handlers for task '{}' (session {}).", required_handlers.size(), task_id, request_id);

    for (const auto& handler_type : required_handlers) {
        auto handler = handler_factory_->create_handler(handler_type);
        if (handler) {
            gateway_->register_handler(handler);
            // TODO: Configure handler mode (Subscriber/Publisher) based on task details.
            // For now, default to subscriber only.
            MessageHandlerBase::configure_handler_mode(handler, HandlerMode::SUBSCRIBER_ONLY);
        } else {
            logger_.Error("gateway_controller.cpp: HandlerFactory failed to create handler of type '{}' for task '{}'.", handler_type, task_id);
        }
    }
}

void GatewayController::on_session_denied(const std::string& request_id, const std::string& reason) {
    logger_.Warn("gateway_controller.cpp: Session with request_id '{}' denied by Bridge: {}", request_id, reason);
    handle_session_teardown(request_id);
}

} // namespace