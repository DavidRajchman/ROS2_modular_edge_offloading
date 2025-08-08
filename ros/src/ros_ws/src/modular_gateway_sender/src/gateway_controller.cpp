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
  this->declare_parameter<std::string>("discovery_service.host", "192.168.65.10");
  this->declare_parameter<int>("discovery_service.port", 9090);
  this->declare_parameter<std::string>("identity.component_type", "V");
  this->declare_parameter<std::string>("identity.component_name", "default_vhc");
  this->declare_parameter<int>("identity.group_id", 60);
  this->declare_parameter<int>("identity.id_in_group", 5);
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
  // Using numeric task IDs as per global configuration
  task_database_["1"] = {"STRING_PROCESSING", {"sensor_msgs/msg/LaserScan"}, {}, {}};
  task_database_["2"] = {"LASER_SCAN_PROCESSING", {"std_msgs/msg/String", "sensor_msgs/msg/LaserScan"}, {}, {}};
  task_database_["3"] = {"STRING_TEST_PIPELINE", {}, {"test/string_input"}, {"test/string_result"}};
  task_database_["4"] = {"BASIC_DATA_PROCESSING", {"test/string_input"}, {}, {}};
  task_database_["5"] = {"SENSOR_FUSION", {"test/string_result"}, {}, {}};
  task_database_["6"] = {"COMPLETE_TEST_SUITE", {}, {"test/string_input"}, {"test/string_result"}};
  
  logger_.Info("gateway_controller.cpp: Initialized mock task database with {} tasks.", task_database_.size());
  // -----------------------------------------

  // Create the ROS 2 services only for VHC components
  if (component_type_ == "V") {
    offloading_service_ = this->create_service<modular_gateway_sender::srv::RequestOffloading>(
      "request_offloading",
      std::bind(&GatewayController::offloading_request_service_handler, this, std::placeholders::_1, std::placeholders::_2)
    );

    terminate_offloading_service_ = this->create_service<modular_gateway_sender::srv::TerminateOffloading>(
      "terminate_offloading",
      std::bind(&GatewayController::terminate_offloading_service_handler, this, std::placeholders::_1, std::placeholders::_2)
    );

    logger_.Info("gateway_controller.cpp: Created ROS services for VHC component type.");
  } else {
    logger_.Info("gateway_controller.cpp: Skipping ROS service creation for component type: {}", component_type_);
  }

  logger_.Info("gateway_controller.cpp: GatewayController basic construction completed. Call initialize() to complete setup.");
}

void GatewayController::initialize() 
{
  logger_.Info("gateway_controller.cpp: Initializing GatewayController components that require shared_from_this()...");

  // Now we can safely call shared_from_this() since the object is fully constructed
  // Create the data plane transport and the RosGateway instance
  auto data_plane_transport = std::make_unique<TcpServerTransport>(data_plane_listen_port_, 1);
  gateway_ = std::make_unique<RosGateway>(this->shared_from_this(), std::move(data_plane_transport));
  gateway_->set_identity(static_cast<uint8_t>(id_group_), static_cast<uint8_t>(identifier_in_group_));

  // Create the handler factory
  handler_factory_ = std::make_unique<HandlerFactory>(gateway_.get(), this->shared_from_this());

  // Start the main control logic thread
  running_ = true;
  control_thread_ = std::thread(&GatewayController::control_thread_func, this);
  
  logger_.Info("gateway_controller.cpp: GatewayController initialization completed. Control thread started.");
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
        response->success = false;
        response->request_id = "";
        response->message = "Gateway is not in OPERATIONAL state.";
        return;
    }

    // Check if the requested task exists in our database
    if (task_database_.find(request->task_id) == task_database_.end()) {
        logger_.Error("gateway_controller.cpp: Offloading request for unknown task_id '{}'. Rejecting.", request->task_id);
        response->success = false;
        response->request_id = "";
        response->message = "Unknown task_id: " + request->task_id;
        return;
    }

    logger_.Info("gateway_controller.cpp: Queuing offloading request for task_id: {}", request->task_id);
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        offloading_request_queue_.push({request->task_id});
    }
    queue_cv_.notify_one();

    // Generate a unique request_id for this session
    std::string new_request_id = std::to_string(request_id_counter_++);
    
    // For now, we reply immediately. In a real system, we might wait for a future/promise.
    response->success = true;
    response->request_id = new_request_id;
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
    const auto discovery_retry_delay = std::chrono::seconds(10);

    while(running_) {
        State current_state = state_.load();
        logger_.Info("gateway_controller.cpp: Control thread processing state: {}", static_cast<int>(current_state));

        switch(current_state) {
            case State::INITIALIZING:
            {
                // Start the data plane server listening for connections
                auto transport = dynamic_cast<TcpServerTransport*>(gateway_->get_transport());
                if (transport && transport->connect()) {
                    logger_.Info("gateway_controller.cpp: Data plane transport started listening on port {}", data_plane_listen_port_);
                    
                    // Start the data plane connection monitoring thread
                    data_plane_connection_thread_ = std::thread(&GatewayController::data_plane_connection_thread_func, this);
                    
                    state_ = State::DISCOVERING;
                } else {
                    logger_.Error("gateway_controller.cpp: Failed to start data plane transport. Retrying in 5s.");
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
                break;
            }
            case State::DISCOVERING:
            {
                logger_.Info("gateway_controller.cpp: Starting discovery client for component type: {}", component_type_);
                
                discovery_protocol::ComponentType comp_type;
                try {
                    logger_.Info("gateway_controller.cpp: Converting component_type '{}' to enum", component_type_);
                    comp_type = discovery_protocol::string_to_component_type(component_type_);
                    logger_.Info("gateway_controller.cpp: Converted to enum value: {}", static_cast<int>(comp_type));
                } catch (const std::exception& e) {
                    logger_.Error("gateway_controller.cpp: Invalid component type '{}': {}", component_type_, e.what());
                    state_ = State::FAILED;
                    break;
                }
                
                // Clean up previous discovery client if it exists
                if (discovery_client_) {
                    discovery_client_->stop();
                    discovery_client_.reset();
                }
                
                discovery_client_ = std::make_unique<DiscoveryClient>();
                if (discovery_client_->start(
                    discovery_host_, discovery_port_, comp_type, component_name_,
                    static_cast<uint8_t>(id_group_), static_cast<uint8_t>(identifier_in_group_), data_plane_listen_port_,
                    std::bind(&GatewayController::on_discovery_success, this, std::placeholders::_1, std::placeholders::_2),
                    std::bind(&GatewayController::on_discovery_failure, this, std::placeholders::_1))) {
                    
                    logger_.Info("gateway_controller.cpp: Discovery client started. Waiting for response...");
                    
                    // Wait for discovery result or timeout
                    std::unique_lock<std::mutex> lock(state_mutex_);
                    if (state_cv_.wait_for(lock, std::chrono::seconds(30), [this] { return state_ != State::DISCOVERING || !running_; })) {
                        // State changed (success or failure handled by callbacks)
                        if (state_ == State::DISCOVERING && running_) {
                            // Still in DISCOVERING state after callback - this means failure occurred
                            logger_.Info("gateway_controller.cpp: Discovery attempt failed. Retrying in {}s...", discovery_retry_delay.count());
                            std::this_thread::sleep_for(discovery_retry_delay);
                        }
                    } else {
                        // Timeout occurred
                        logger_.Warn("gateway_controller.cpp: Discovery attempt timed out. Retrying in {}s...", discovery_retry_delay.count());
                        std::this_thread::sleep_for(discovery_retry_delay);
                    }
                } else {
                    logger_.Error("gateway_controller.cpp: Failed to start discovery client. Retrying in {}s...", discovery_retry_delay.count());
                    std::this_thread::sleep_for(discovery_retry_delay);
                }
                break;
            }
            case State::CONNECTING_TO_BRIDGE:
            {
                logger_.Info("gateway_controller.cpp: State transition: Connecting to Bridge Control Plane at {}:{}", bridge_cp_host_, bridge_cp_port_);
                
                bridge_cp_client_ = std::make_unique<BridgeCpClient>();
                if (bridge_cp_client_->start(
                    bridge_cp_host_, bridge_cp_port_,
                    std::bind(&GatewayController::on_dp_confirmed, this),
                    std::bind(&GatewayController::on_session_approved, this, std::placeholders::_1),
                    std::bind(&GatewayController::on_session_denied, this, std::placeholders::_1, std::placeholders::_2))) {
                    
                    // Send the DP_INFO message immediately
                    logger_.Info("gateway_controller.cpp: Bridge CP client connected successfully. Sending DP_INFO message...");
                    bridge_cp_client_->send_dp_info(component_id_, "0.0.0.0", data_plane_listen_port_);
                    logger_.Info("gateway_controller.cpp: DP_INFO sent. State transition: CONNECTING_TO_BRIDGE -> WAITING_FOR_DP_CONNECTION");
                    state_ = State::WAITING_FOR_DP_CONNECTION;
                } else {
                    logger_.Error("gateway_controller.cpp: Failed to start Bridge CP client. Retrying in 10s.");
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                }
                break;
            }
            case State::WAITING_FOR_DP_CONNECTION:
            {
                logger_.Info("gateway_controller.cpp: Waiting for DP_CONNECTION_CONFIRMED message from Bridge CP...");
                std::unique_lock<std::mutex> lock(state_mutex_);
                
                // Wait with a timeout so we can periodically log our status
                if (state_cv_.wait_for(lock, std::chrono::seconds(30), [this] { return state_ != State::WAITING_FOR_DP_CONNECTION || !running_; })) {
                    if (state_ == State::OPERATIONAL) {
                        logger_.Info("gateway_controller.cpp: Successfully received DP_CONNECTION_CONFIRMED and transitioned to OPERATIONAL");
                    }
                } else {
                    logger_.Warn("gateway_controller.cpp: Still waiting for DP_CONNECTION_CONFIRMED from Bridge CP (30s timeout reached)");
                }
                break;
            }
            case State::OPERATIONAL:
            {
                logger_.Debug("gateway_controller.cpp: In OPERATIONAL state, checking for work...");
                
                // Process any queued offloading or termination requests
                std::unique_lock<std::mutex> lock(queue_mutex_);
                if (queue_cv_.wait_for(lock, keepalive_interval, [this] { 
                    bool has_work = !termination_request_queue_.empty() || !running_;
                    if (component_type_ == "V") {
                        has_work = has_work || !offloading_request_queue_.empty();
                    }
                    return has_work;
                })) {
                    
                    // Process offloading requests (VHC only)
                    if (component_type_ == "V") {
                        while (!offloading_request_queue_.empty() && running_) {
                            auto req = offloading_request_queue_.front();
                            offloading_request_queue_.pop();
                            lock.unlock();
                            
                            std::string request_id = std::to_string(request_id_counter_++);
                            {
                                std::lock_guard<std::mutex> session_lock(session_mutex_);
                                active_sessions_[request_id] = {request_id, req.task_id, std::chrono::steady_clock::now()};
                                logger_.Info("gateway_controller.cpp: Created new active session '{}' for task '{}'", request_id, req.task_id);
                            }
                            
                            auto task_it = task_database_.find(req.task_id);
                            if (task_it != task_database_.end()) {
                                bridge_cp_client_->send_offload_request(component_id_, request_id, req.task_id, task_it->second.task_name);
                                logger_.Info("gateway_controller.cpp: Sent offload request for task '{}' with request_id '{}'", req.task_id, request_id);
                            }
                            
                            lock.lock();
                        }
                    }
                    
                    // Process termination requests
                    while (!termination_request_queue_.empty() && running_) {
                        auto req_id = termination_request_queue_.front();
                        termination_request_queue_.pop();
                        lock.unlock();
                        
                        bridge_cp_client_->send_session_terminate_request(component_id_, req_id);
                        handle_session_teardown(req_id);
                        logger_.Info("gateway_controller.cpp: Sent termination request for session '{}'", req_id);
                        
                        lock.lock();
                    }
                } else {
                    // Timeout occurred, send keepalives for active sessions
                    lock.unlock();
                    
                    std::lock_guard<std::mutex> session_lock(session_mutex_);
                    auto now = std::chrono::steady_clock::now();
                    
                    if (active_sessions_.empty()) {
                        logger_.Debug("gateway_controller.cpp: No active sessions to send keepalives for");
                    } else {
                        logger_.Info("gateway_controller.cpp: Checking {} active sessions for keepalive requirements", active_sessions_.size());
                    }
                    
                    for (auto& [req_id, session] : active_sessions_) {
                        if (now - session.last_keepalive_sent >= keepalive_interval) {
                            logger_.Info("gateway_controller.cpp: Sending keepalive for session '{}' (last sent {}s ago)", 
                                        req_id, std::chrono::duration_cast<std::chrono::seconds>(now - session.last_keepalive_sent).count());
                            bridge_cp_client_->send_session_keepalive(component_id_, req_id);
                            session.last_keepalive_sent = now;
                        } else {
                            auto time_until_next = keepalive_interval - (now - session.last_keepalive_sent);
                            logger_.Debug("gateway_controller.cpp: Session '{}' keepalive not due yet ({}s remaining)", 
                                         req_id, std::chrono::duration_cast<std::chrono::seconds>(time_until_next).count());
                        }
                    }
                }
                break;
            }
            case State::FAILED:
            {
                logger_.Error("gateway_controller.cpp: Controller in FAILED state. Exiting control loop.");
                running_ = false;
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
    logger_.Error("gateway_controller.cpp: No transport available for data plane monitoring.");
    return;
  }

  while (running_) {
    auto tcp_server = dynamic_cast<TcpServerTransport*>(transport);
    if (tcp_server && tcp_server->accept_connection()) {
      logger_.Info("gateway_controller.cpp: Data plane connection accepted from Bridge.");
      // After a successful accept, do not log again until a new connection occurs (accept_connection will return false while connected).
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
}

void GatewayController::handle_session_teardown(const std::string& request_id) {
    std::lock_guard<std::mutex> lock(session_mutex_);
    auto session_it = active_sessions_.find(request_id);
    if (session_it == active_sessions_.end()) {
        logger_.Warn("gateway_controller.cpp: Attempted to teardown unknown session '{}'", request_id);
        return;
    }

    const std::string& task_id = session_it->second.task_id;
    auto task_it = task_database_.find(task_id);
    if (task_it != task_database_.end()) {
        const auto& required_handlers = task_it->second.required_handlers;
        logger_.Info("gateway_controller.cpp: Deactivating {} handlers for session '{}'", required_handlers.size(), request_id);
        
        for (const auto& handler_type : required_handlers) {
            auto handler = handler_factory_->create_handler(handler_type);
            if (handler) {
                gateway_->unregister_handler(handler->get_name());
                logger_.Info("gateway_controller.cpp: Deactivated handler '{}'", handler_type);
            }
        }
    } else {
        logger_.Error("gateway_controller.cpp: Session '{}' references unknown task_id '{}'", request_id, task_id);
    }

    active_sessions_.erase(session_it);
    logger_.Info("gateway_controller.cpp: Session '{}' removed.", request_id);
}

// --- Callback Implementations ---

void GatewayController::on_discovery_success(const std::string& bridge_host, int bridge_port) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ == State::DISCOVERING) {
        bridge_cp_host_ = bridge_host;
        bridge_cp_port_ = bridge_port;
        state_ = State::CONNECTING_TO_BRIDGE;
        logger_.Info("gateway_controller.cpp: Discovery successful. Bridge found at {}:{}", bridge_host, bridge_port);
    }
    state_cv_.notify_one();
}

void GatewayController::on_discovery_failure(const std::string& error_message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    logger_.Error("gateway_controller.cpp: Discovery failed: {}", error_message);
    // Do not change state here - let the control thread handle the retry logic
    // Just notify the condition variable to wake up the control thread
    state_cv_.notify_one();
}

void GatewayController::on_dp_confirmed() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    logger_.Info("gateway_controller.cpp: Received DP_CONNECTION_CONFIRMED from Bridge CP");
    
    if (state_ == State::WAITING_FOR_DP_CONNECTION) {
        logger_.Info("gateway_controller.cpp: State transition: WAITING_FOR_DP_CONNECTION -> OPERATIONAL");
        gateway_->start_receiver();
        state_ = State::OPERATIONAL;
        logger_.Info("gateway_controller.cpp: Data plane confirmed. Gateway is now OPERATIONAL and ready to handle sessions.");
    } else {
        logger_.Warn("gateway_controller.cpp: Received DP confirmation in unexpected state: {} (expected WAITING_FOR_DP_CONNECTION)", 
                    static_cast<int>(state_.load()));
    }
    state_cv_.notify_one();
}

void GatewayController::on_session_approved(const nlohmann::json& payload) {
    // Extract request_id for logging and potential session lookup
    std::string request_id;
    if (payload.contains("request_id")) {
        request_id = payload["request_id"];
        logger_.Info("gateway_controller.cpp: Session with request_id '{}' approved by Bridge.", request_id);
    } else {
        logger_.Warn("gateway_controller.cpp: SESSION_APPROVED received without request_id");
        request_id = "unknown";
    }
    
    std::lock_guard<std::mutex> lock(session_mutex_);
    
    std::string task_id;
    
    // Check if this is a VHC case (local session exists) or MEC case (unsolicited SESSION_APPROVED)
    auto session_it = active_sessions_.find(request_id);
    if (session_it != active_sessions_.end()) {
        // VHC case: Look up task_id from local session map
        task_id = session_it->second.task_id;
        logger_.Info("gateway_controller.cpp: VHC case - found task_id '{}' in local session map", task_id);
        
        // Update the keepalive timestamp to start the timer now that it's approved
        session_it->second.last_keepalive_sent = std::chrono::steady_clock::now();
    } else {
        // MEC case: Extract task_id directly from SESSION_APPROVED payload
        if (payload.contains("task_id")) {
            task_id = payload["task_id"];
            logger_.Info("gateway_controller.cpp: MEC case - extracted task_id '{}' from SESSION_APPROVED payload", task_id);
        } else {
            logger_.Error("gateway_controller.cpp: MEC SESSION_APPROVED missing task_id in payload");
            return;
        }
    }

    auto task_it = task_database_.find(task_id);
    if (task_it == task_database_.end()) {
        logger_.Error("gateway_controller.cpp: Session '{}' references unknown task_id '{}'", request_id, task_id);
        return;
    }

    const auto& task_details = task_it->second;
    
    // Determine which handlers to create and their modes
    std::vector<std::pair<std::string, HandlerMode>> handlers_to_create;
    
    if (!task_details.input_handlers.empty() || !task_details.output_handlers.empty()) {
        // New input/output oriented task
        logger_.Info("gateway_controller.cpp: Processing input/output oriented task '{}' with {} input and {} output handlers", 
                    task_id, task_details.input_handlers.size(), task_details.output_handlers.size());
        
        if (component_type_ == "V") {
            // VHC: Subscribe to inputs, publish outputs
            for (const auto& handler_type : task_details.input_handlers) {
                handlers_to_create.emplace_back(handler_type, HandlerMode::SUBSCRIBER_ONLY);
            }
            for (const auto& handler_type : task_details.output_handlers) {
                handlers_to_create.emplace_back(handler_type, HandlerMode::PUBLISHER_ONLY);
            }
        } else if (component_type_ == "M") {
            // MEC: Publish to inputs, subscribe to outputs  
            for (const auto& handler_type : task_details.input_handlers) {
                handlers_to_create.emplace_back(handler_type, HandlerMode::PUBLISHER_ONLY);
            }
            for (const auto& handler_type : task_details.output_handlers) {
                handlers_to_create.emplace_back(handler_type, HandlerMode::SUBSCRIBER_ONLY);
            }
        } else {
            // Unknown component type - use both modes for all handlers
            for (const auto& handler_type : task_details.input_handlers) {
                handlers_to_create.emplace_back(handler_type, HandlerMode::BOTH);
            }
            for (const auto& handler_type : task_details.output_handlers) {
                handlers_to_create.emplace_back(handler_type, HandlerMode::BOTH);
            }
        }
    } else {
        // Legacy task using required_handlers (backward compatibility)
        logger_.Info("gateway_controller.cpp: Processing legacy task '{}' with {} required handlers", 
                    task_id, task_details.required_handlers.size());
        
        HandlerMode mode = HandlerMode::BOTH;
        if (component_type_ == "V") {
            mode = HandlerMode::SUBSCRIBER_ONLY;
        } else if (component_type_ == "M") {
            mode = HandlerMode::PUBLISHER_ONLY;
        }
        
        for (const auto& handler_type : task_details.required_handlers) {
            handlers_to_create.emplace_back(handler_type, mode);
        }
    }

    logger_.Info("gateway_controller.cpp: Activating {} handlers for task '{}' (session {}).", handlers_to_create.size(), task_id, request_id);

    for (const auto& [handler_type, mode] : handlers_to_create) {
        auto handler = handler_factory_->create_handler(handler_type);
        if (handler) {
            MessageHandlerBase::configure_handler_mode(handler, mode);
            gateway_->register_handler(handler);
            logger_.Info("gateway_controller.cpp: Activated handler '{}' with mode {}", handler_type, static_cast<int>(mode));
        } else {
            logger_.Error("gateway_controller.cpp: Failed to create handler for type '{}'", handler_type);
        }
    }
}

void GatewayController::on_session_denied(const std::string& request_id, const std::string& reason) {
    logger_.Warn("gateway_controller.cpp: Session with request_id '{}' denied by Bridge: {}", request_id, reason);
    handle_session_teardown(request_id);
}

} // namespace gateway