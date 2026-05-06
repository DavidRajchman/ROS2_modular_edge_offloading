#include "modular_gateway_sender/gateway_controller.hpp"
#include "modular_gateway_sender/discovery_client.hpp"
#include "discovery_protocol/protocol.hpp"
#include "modular_gateway_sender/handler_factory.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>

namespace gateway {

// Convert numeric message type ID from config JSON to MessageType enum
// Returns false if ID is unknown
bool GatewayController::message_type_from_id(uint32_t id, MessageType& out) {
    switch (id) {
        case 1: out = MessageType::STRING; return true;
        case 2: out = MessageType::INT32; return true;
        case 3: out = MessageType::FLOAT32; return true;
        case 4: out = MessageType::BOOL; return true;
        case 11: out = MessageType::smLASERSCAN; return true;
        case 201: out = MessageType::STRING_TEST_INPUT; return true;
        case 202: out = MessageType::STRING_TEST_RESULT; return true;
        default: return false;
    }
}

// Parse global configuration JSON received from DiscoveryService
// Populates task database with available tasks and their message types
// Returns true if at least one task was successfully loaded
bool GatewayController::load_global_config_from_json(const std::string& config_json) {
    logger_.Info("gateway_controller.cpp: Parsing global config JSON ({} bytes)", config_json.size());
    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(config_json);
    } catch (const std::exception& e) {
        logger_.Error("gateway_controller.cpp: Failed to parse global config JSON: {}", e.what());
        return false;
    }

    // Clear existing task database before loading new config
    task_database_.clear();

    // Extract system-wide session parameters
    if (doc.contains("default_session_timeout")) {
        default_session_timeout_ = doc["default_session_timeout"].get<int>();
    }
    if (doc.contains("max_concurrent_sessions")) {
        max_concurrent_sessions_ = doc["max_concurrent_sessions"].get<int>();
    }

    // Validate that tasks array exists in config
    if (!doc.contains("available_tasks") || !doc["available_tasks"].is_array()) {
        logger_.Error("gateway_controller.cpp: Global config missing 'available_tasks' array");
        return false;
    }

    // Parse each task definition from the config
    int loaded = 0;
    for (const auto& t : doc["available_tasks"]) {
        if (!t.contains("task_id") || !t.contains("task_name")) {
            logger_.Warn("gateway_controller.cpp: Skipping task with missing id/name");
            continue;
        }
        std::string key = std::to_string(t["task_id"].get<int>());
        TaskDetails details;
        details.task_name = t["task_name"].get<std::string>();

        // Lambda to parse message type ID arrays and convert to MessageType enums
        auto parse_types = [&](const char* field, std::vector<MessageType>& outv){
            if (!t.contains(field)) return; 
            for (const auto& val : t[field]) {
                uint32_t id = val.get<uint32_t>();
                MessageType mt;
                if (message_type_from_id(id, mt)) {
                    outv.push_back(mt);
                } else {
                    logger_.Warn("gateway_controller.cpp: Unknown message type id {} in task {}", id, key);
                }
            }
        };

        // Parse input and output message types for this task
        parse_types("input_message_types", details.input_types);
        parse_types("output_message_types", details.output_types);

        task_database_[key] = std::move(details);
        ++loaded;
    }

    logger_.Info("gateway_controller.cpp: Loaded {} tasks from global config. defaults: timeout={}s, max_sessions={}",
                             loaded, default_session_timeout_, max_concurrent_sessions_);
    return loaded > 0;
}

bool GatewayController::load_local_config(const std::string& path) {
    logger_.Info("gateway_controller.cpp: Loading local P2P config from '{}'", path);
    std::ifstream file(path);
    if (!file.is_open()) {
        logger_.Error("gateway_controller.cpp: Failed to open local config file '{}'", path);
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string config_json = buffer.str();
    
    if (!load_global_config_from_json(config_json)) {
        logger_.Error("gateway_controller.cpp: Failed to parse local config JSON from '{}'", path);
        return false;
    }
    return true;
}

void GatewayController::activate_all_p2p_sessions() {
    logger_.Info("gateway_controller.cpp: Auto-activating all tasks from local config for P2P mode.");
    for (const auto& [task_id, details] : task_database_) {
        std::string request_id = "p2p_session_" + task_id;
        
        {
            std::lock_guard<std::mutex> session_lock(session_mutex_);
            active_sessions_[request_id] = {request_id, task_id, std::chrono::steady_clock::now()};
        }
        
        nlohmann::json mock_payload = {
            {"request_id", request_id},
            {"task_id", task_id}
        };
        logger_.Info("gateway_controller.cpp: [P2P] Manually approving auto-session '{}' for task '{}'", request_id, task_id);
        on_session_approved(mock_payload);
    }
}

// Constructor: Set up ROS2 node with parameters and services
// Note: Cannot use shared_from_this() here - call initialize() after construction
GatewayController::GatewayController(const rclcpp::NodeOptions& options)
  : rclcpp::Node("gateway_controller", options),
    logger_(CppLogging::Logger("gateway"))
{
  logger_.Info("gateway_controller.cpp: Constructing GatewayController...");

  // Declare ROS parameters with defaults (can be overridden by launch files)
  this->declare_parameter<std::string>("discovery_service.host", "192.168.65.10");
  this->declare_parameter<int>("discovery_service.port", 9090);
  this->declare_parameter<std::string>("identity.component_type", "V");
  this->declare_parameter<std::string>("identity.component_name", "default_vhc");
  this->declare_parameter<int>("identity.group_id", 60);
  this->declare_parameter<int>("identity.id_in_group", 5);
  this->declare_parameter<int>("data_plane.listen_port", 7401);
  this->declare_parameter<std::string>("operation_mode", "networked");
  this->declare_parameter<std::string>("p2p.peer_host", "127.0.0.1");
  this->declare_parameter<int>("p2p.peer_port", 7401);
  this->declare_parameter<std::string>("p2p.local_config_path", "");

  discovery_host_ = this->get_parameter("discovery_service.host").as_string();
  discovery_port_ = this->get_parameter("discovery_service.port").as_int();
  component_type_ = this->get_parameter("identity.component_type").as_string();
  component_name_ = this->get_parameter("identity.component_name").as_string();
  id_group_ = this->get_parameter("identity.group_id").as_int();
  identifier_in_group_ = this->get_parameter("identity.id_in_group").as_int();
  data_plane_listen_port_ = this->get_parameter("data_plane.listen_port").as_int();
  operation_mode_ = this->get_parameter("operation_mode").as_string();
  p2p_peer_host_ = this->get_parameter("p2p.peer_host").as_string();
  p2p_peer_port_ = this->get_parameter("p2p.peer_port").as_int();
  local_config_path_ = this->get_parameter("p2p.local_config_path").as_string();
  component_id_ = std::to_string(id_group_) + ":" + std::to_string(identifier_in_group_);

  // Task database will be populated from DiscoveryService global config after registration

  // Create ROS services only for VHC (Vehicle) components to request/terminate offloading
  // MEC (Edge) components don't need these services as they receive unsolicited SESSION_APPROVED
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

// Complete initialization after construction (required for shared_from_this())
// Creates data plane transport, gateway, handler factory, and starts control thread
void GatewayController::initialize() 
{
  logger_.Info("gateway_controller.cpp: Initializing GatewayController components that require shared_from_this()...");

  // Create Data Plane Transport based on operation mode
  std::unique_ptr<TransportBase> data_plane_transport;

  if (operation_mode_ == "networked") {
      // Traditional networked mode: Always a TCP server (Bridge connects to us)
      logger_.Info("gateway_controller.cpp: mode=networked. Creating TcpServerTransport on port {}", data_plane_listen_port_);
      data_plane_transport = std::make_unique<TcpServerTransport>(data_plane_listen_port_, 1);
  } else {
      // P2P or P2P_DS mode: MEC is Server, VHC is Client
      if (component_type_ == "M") {
          logger_.Info("gateway_controller.cpp: mode={}. MEC role: Creating TcpServerTransport on port {}", 
                       operation_mode_, data_plane_listen_port_);
          data_plane_transport = std::make_unique<TcpServerTransport>(data_plane_listen_port_, 1);
      } else {
          // VHC or other: TCP Client
          // In 'p2p' mode, host/port are static. In 'p2p_ds', they will be updated after discovery.
          std::string target_host = (operation_mode_ == "p2p") ? p2p_peer_host_ : "0.0.0.0";
          int target_port = (operation_mode_ == "p2p") ? p2p_peer_port_ : 0;
          
          logger_.Info("gateway_controller.cpp: mode={}. VHC role: Creating TcpClientTransport (target {}:{})", 
                       operation_mode_, target_host, target_port);
          data_plane_transport = std::make_unique<TcpClientTransport>(target_host, target_port);
      }
  }

  gateway_ = std::make_unique<RosGateway>(this->shared_from_this(), std::move(data_plane_transport));
  gateway_->set_identity(static_cast<uint8_t>(id_group_), static_cast<uint8_t>(identifier_in_group_));

  // Create factory for message handlers (bridges ROS topics to data plane)
  handler_factory_ = std::make_unique<HandlerFactory>(gateway_.get(), this->shared_from_this());

  // Launch state machine thread for discovery, bridge connection, and session management
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

// ROS service handler: VHC application requests task offloading
// Validates task exists and gateway is ready, then queues request for control thread
// Returns immediately with request_id (actual approval happens asynchronously)
void GatewayController::offloading_request_service_handler(
  const std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Request> request,
  std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Response> response)
{
    // Reject if not fully connected to Bridge and operational
    if (state_.load() != State::OPERATIONAL) {
        logger_.Warn("gateway_controller.cpp: Offloading request for task '{}' received, but controller is not operational. Rejecting.", request->task_id);
        response->success = false;
        response->request_id = "";
        response->message = "Gateway is not in OPERATIONAL state.";
        return;
    }

    // Validate task_id exists in global configuration
    if (task_database_.find(request->task_id) == task_database_.end()) {
        logger_.Error("gateway_controller.cpp: Offloading request for unknown task_id '{}'. Rejecting.", request->task_id);
        response->success = false;
        response->request_id = "";
        response->message = "Unknown task_id: " + request->task_id;
        return;
    }

    logger_.Info("gateway_controller.cpp: Queuing offloading request for task_id: {}, vhc_data: '{}'", request->task_id, request->vhc_data);
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        offloading_request_queue_.push({request->task_id, request->vhc_data});
    }
    queue_cv_.notify_one();  // Wake control thread to process request

    // Generate unique request_id for tracking this session
    std::string new_request_id = std::to_string(request_id_counter_++);
    
    // Return immediately - actual approval happens asynchronously via Bridge/OM
    response->success = true;
    response->request_id = new_request_id;
    response->message = "Request queued for processing by the bridge.";
}

// ROS service handler: VHC application requests graceful session termination
// Queues termination request for control thread to send to Bridge/OM
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
    queue_cv_.notify_one();  // Wake control thread to process termination

    response->success = true;
    response->message = "Termination request queued.";
}

// Main state machine thread: manages lifecycle from discovery through operational state
// Handles: Discovery → Bridge connection → DP confirmation → Session management
void GatewayController::control_thread_func() {
    const auto keepalive_interval = std::chrono::seconds(15);      // Send session keepalives every 15s
    const auto discovery_retry_delay = std::chrono::seconds(10);   // Retry discovery every 10s on failure

    while(running_) {
        State current_state = state_.load();
        logger_.Info("gateway_controller.cpp: Control thread processing state: {}", static_cast<int>(current_state));

        switch(current_state) {
            case State::INITIALIZING:
            {
                // Load local configuration for P2P modes
                if (operation_mode_ != "networked") {
                    if (!local_config_path_.empty()) {
                        if (!load_local_config(local_config_path_)) {
                            RCLCPP_FATAL(this->get_logger(), "Failed to load required P2P configuration. Exiting.");
                            state_ = State::FAILED;
                            break;
                        }
                    } else {
                        RCLCPP_FATAL(this->get_logger(), "No local_config_path provided for P2P mode. Exiting.");
                        state_ = State::FAILED;
                        break;
                    }
                }

                // Setup transport connection/listening
                if (operation_mode_ == "networked" || component_type_ == "M") {
                    // TCP Server path (Networked Bridge connection or MEC P2P listener)
                    auto transport = dynamic_cast<TcpServerTransport*>(gateway_->get_transport());
                    if (transport && transport->connect()) {
                        logger_.Info("gateway_controller.cpp: Server transport started on port {}", data_plane_listen_port_);
                        data_plane_connection_thread_ = std::thread(&GatewayController::data_plane_connection_thread_func, this);
                        
                        if (operation_mode_ == "networked" || operation_mode_ == "p2p_ds") {
                            state_ = State::DISCOVERING;
                        } else {
                            // MEC P2P: No discovery needed, skip to waiting for peer connection
                            state_ = State::WAITING_FOR_DP_CONNECTION;
                        }
                    } else {
                        logger_.Error("gateway_controller.cpp: Failed to start server transport. Retrying in 5s.");
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    }
                } else {
                    // VHC P2P path (TCP Client)
                    if (operation_mode_ == "p2p_ds") {
                        state_ = State::DISCOVERING; // Resolve MEC IP first
                    } else {
                        // Static P2P: Go straight to connection attempts
                        state_ = State::WAITING_FOR_DP_CONNECTION;
                    }
                }
                break;
            }
            case State::DISCOVERING:
            {
                // Only Networked or P2P_DS modes reach here
                logger_.Info("gateway_controller.cpp: Starting discovery client for component type: {}", component_type_);
                
                discovery_protocol::ComponentType comp_type;
                try {
                    comp_type = discovery_protocol::string_to_component_type(component_type_);
                } catch (const std::exception& e) {
                    logger_.Error("gateway_controller.cpp: Invalid component type '{}': {}", component_type_, e.what());
                    state_ = State::FAILED;
                    break;
                }
                
                if (discovery_client_) {
                    discovery_client_->stop();
                    discovery_client_.reset();
                }
                
                discovery_client_ = std::make_unique<DiscoveryClient>();
                if (discovery_client_->start(
                    discovery_host_, discovery_port_, comp_type, component_name_,
                    static_cast<uint8_t>(id_group_), static_cast<uint8_t>(identifier_in_group_), data_plane_listen_port_,
                    std::bind(&GatewayController::on_discovery_success, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3),
                    std::bind(&GatewayController::on_discovery_failure, this, std::placeholders::_1))) {
                    
                    logger_.Info("gateway_controller.cpp: Discovery client started. Waiting for response...");
                    
                    std::unique_lock<std::mutex> lock(state_mutex_);
                    if (state_cv_.wait_for(lock, std::chrono::seconds(30), [this] { return state_ != State::DISCOVERING || !running_; })) {
                        if (state_ == State::DISCOVERING && running_) {
                            logger_.Info("gateway_controller.cpp: Discovery attempt failed. Retrying in {}s...", discovery_retry_delay.count());
                            std::this_thread::sleep_for(discovery_retry_delay);
                        }
                    } else {
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
                // Networked mode only
                logger_.Info("gateway_controller.cpp: State transition: Connecting to Bridge Control Plane at {}:{}", bridge_cp_host_, bridge_cp_port_);
                
                bridge_cp_client_ = std::make_unique<BridgeCpClient>();
                if (bridge_cp_client_->start(
                    bridge_cp_host_, bridge_cp_port_,
                    std::bind(&GatewayController::on_dp_confirmed, this),
                    std::bind(&GatewayController::on_session_approved, this, std::placeholders::_1),
                    std::bind(&GatewayController::on_session_denied, this, std::placeholders::_1, std::placeholders::_2))) {
                    
                    logger_.Info("gateway_controller.cpp: Bridge CP client connected successfully. Sending DP_INFO message...");
                    bridge_cp_client_->send_dp_info(component_id_, "0.0.0.0", data_plane_listen_port_);
                    state_ = State::WAITING_FOR_DP_CONNECTION;
                } else {
                    logger_.Error("gateway_controller.cpp: Failed to start Bridge CP client. Retrying in 10s.");
                    std::this_thread::sleep_for(std::chrono::seconds(10));
                }
                break;
            }
            case State::WAITING_FOR_DP_CONNECTION:
            {
                // For VHC P2P/P2P_DS: Perform proactive connection
                if (operation_mode_ != "networked" && component_type_ == "V") {
                    auto transport = dynamic_cast<TcpClientTransport*>(gateway_->get_transport());
                    if (transport) {
                        if (transport->connect()) {
                            logger_.Info("gateway_controller.cpp: [P2P] Successfully connected to peer MEC.");
                            on_dp_confirmed(); // Manually trigger OPERATIONAL transition
                        } else {
                            logger_.Warn("gateway_controller.cpp: [P2P] Peer MEC not reached, retrying in 5s...");
                            std::this_thread::sleep_for(std::chrono::seconds(5));
                        }
                    }
                }

                logger_.Info("gateway_controller.cpp: Waiting for data plane connection...");
                std::unique_lock<std::mutex> lock(state_mutex_);
                state_cv_.wait_for(lock, std::chrono::seconds(30), [this] { return state_ != State::WAITING_FOR_DP_CONNECTION || !running_; });
                break;
            }
            case State::OPERATIONAL:
            {
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
                            }
                            
                            if (operation_mode_ == "networked" && bridge_cp_client_) {
                                auto task_it = task_database_.find(req.task_id);
                                if (task_it != task_database_.end()) {
                                    bridge_cp_client_->send_offload_request(component_id_, request_id, req.task_id, task_it->second.task_name, req.vhc_data);
                                    logger_.Info("gateway_controller.cpp: Sent offload request for task '{}' with request_id '{}', vhc_data: '{}'", req.task_id, request_id, req.vhc_data);
                                }
                            } else {
                                logger_.Warn("gateway_controller.cpp: [P2P] Ignored dynamic offloading request '{}' for task '{}'. P2P mode auto-activates all tasks.", request_id, req.task_id);
                            }
                            
                            lock.lock();
                        }
                    }
                    
                    // Process termination requests
                    while (!termination_request_queue_.empty() && running_) {
                        auto req_id = termination_request_queue_.front();
                        termination_request_queue_.pop();
                        lock.unlock();
                        
                        if (operation_mode_ == "networked" && bridge_cp_client_) {
                            bridge_cp_client_->send_session_terminate_request(component_id_, req_id);
                        }
                        handle_session_teardown(req_id);
                        
                        lock.lock();
                    }
                } else {
                    // Timeout occurred, send keepalives for active sessions (Networked only)
                    lock.unlock();
                    
                    if (operation_mode_ == "networked" && bridge_cp_client_) {
                        std::lock_guard<std::mutex> session_lock(session_mutex_);
                        auto now = std::chrono::steady_clock::now();
                        for (auto& [req_id, session] : active_sessions_) {
                            if (now - session.last_keepalive_sent >= keepalive_interval) {
                                bridge_cp_client_->send_session_keepalive(component_id_, req_id);
                                session.last_keepalive_sent = now;
                            }
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
      logger_.Info("gateway_controller.cpp: Data plane connection accepted.");
      
      // In P2P modes, we don't have a Bridge CP to confirm the connection,
      // so we transition to OPERATIONAL immediately upon TCP accept.
      if (operation_mode_ != "networked") {
          on_dp_confirmed();
      }
      
      // After a successful accept, do not log again until a new connection occurs (accept_connection will return false while connected).
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  }
}

// Clean up session: deactivate handlers and remove from active session map
// Called on termination request or session denial
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
        const auto& td = task_it->second;
        // Deactivate all handlers associated with this task
        if (!td.input_types.empty() || !td.output_types.empty()) {
            // Modern path: deactivate by MessageType enum
            auto deactivate = [&](const std::vector<MessageType>& list){
                for (auto mt : list) {
                    auto handler = handler_factory_->get_or_create(mt);
                    if (handler) {
                        gateway_->unregister_handler(handler->get_name());
                        logger_.Info("gateway_controller.cpp: Deactivated handler for msgType {}", static_cast<int>(mt));
                    }
                }
            };
            deactivate(td.input_types);
            deactivate(td.output_types);
        } else {
            // Legacy path: deactivate by handler type string

            const auto& required_handlers = td.required_handlers;
            logger_.Info("gateway_controller.cpp: Deactivating {} legacy handlers for session '{}'", required_handlers.size(), request_id);
            for (const auto& handler_type : required_handlers) {
                auto handler = handler_factory_->create_handler(handler_type);
                if (handler) {
                    gateway_->unregister_handler(handler->get_name());
                    logger_.Info("gateway_controller.cpp: Deactivated legacy handler '{}'", handler_type);
                }
            }
        }
    } else {
        logger_.Error("gateway_controller.cpp: Session '{}' references unknown task_id '{}'", request_id, task_id);
    }

    active_sessions_.erase(session_it);
    logger_.Info("gateway_controller.cpp: Session '{}' removed.", request_id);
}

// --- Discovery and Bridge Connection Callbacks ---

// Called by DiscoveryClient when registration succeeds
// Receives Bridge address and global configuration JSON
void GatewayController::on_discovery_success(const std::string& bridge_host, int bridge_port, const std::string& config_json) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ == State::DISCOVERING) {
        if (operation_mode_ == "p2p_ds") {
            logger_.Info("gateway_controller.cpp: Discovery successful. Peer MEC found at {}:{}", bridge_host, bridge_port);
            auto transport = dynamic_cast<TcpClientTransport*>(gateway_->get_transport());
            if (transport) {
                transport->set_target(bridge_host, bridge_port);
            }
            state_ = State::WAITING_FOR_DP_CONNECTION;
        } else {
            bridge_cp_host_ = bridge_host;
            bridge_cp_port_ = bridge_port;
            // Load task database from global config before connecting to Bridge
            if (!config_json.empty()) {
                bool ok = load_global_config_from_json(config_json);
                if (!ok) {
                    logger_.Error("gateway_controller.cpp: Failed to parse global config JSON from DiscoveryService. Staying in FAILED state.");
                    state_ = State::FAILED;
                    state_cv_.notify_one();
                    return;
                }
            } else {
                logger_.Error("gateway_controller.cpp: DiscoveryService returned empty config JSON. Cannot continue without task database.");
                state_ = State::FAILED;
                state_cv_.notify_one();
                return;
            }
            state_ = State::CONNECTING_TO_BRIDGE;
            logger_.Info("gateway_controller.cpp: Discovery successful. Bridge found at {}:{}", bridge_host, bridge_port);
        }
    }
    state_cv_.notify_one();
}

// Called by DiscoveryClient when registration fails
// Does not change state - lets control thread retry with backoff
void GatewayController::on_discovery_failure(const std::string& error_message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    logger_.Error("gateway_controller.cpp: Discovery failed: {}", error_message);
    // Keep state in DISCOVERING - control thread will retry after delay
    state_cv_.notify_one();
}

// Called by BridgeCpClient when Bridge confirms data plane connection established
// Transitions to OPERATIONAL state and starts receiving binary messages
void GatewayController::on_dp_confirmed() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    logger_.Info("gateway_controller.cpp: Data Plane connection confirmed");
    
    if (state_ == State::WAITING_FOR_DP_CONNECTION) {
        logger_.Info("gateway_controller.cpp: State transition: WAITING_FOR_DP_CONNECTION -> OPERATIONAL");
        gateway_->start_receiver();  // Begin processing data plane messages
        state_ = State::OPERATIONAL;
        logger_.Info("gateway_controller.cpp: Data plane confirmed. Gateway is now OPERATIONAL and ready to handle sessions.");
        
        if (operation_mode_ != "networked") {
            activate_all_p2p_sessions();
        }
    } else {
        logger_.Warn("gateway_controller.cpp: Received DP confirmation in unexpected state: {} (expected WAITING_FOR_DP_CONNECTION)", 
                    static_cast<int>(state_.load()));
    }
    state_cv_.notify_one();
}

// Called by BridgeCpClient when OM approves offloading session
// VHC: activates subscribers (send data) and publishers (receive results)
// MEC: activates publishers (receive data) and subscribers (send results)
void GatewayController::on_session_approved(const nlohmann::json& payload) {
    // Extract request_id from approval message
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
    
    // Determine task_id: VHC has local session, MEC receives unsolicited approval
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
    
    // Activate handlers based on component role (VHC vs MEC)
    // Modern path uses MessageType enums; legacy path uses handler type strings
    if (!task_details.input_types.empty() || !task_details.output_types.empty()) {
        logger_.Info("gateway_controller.cpp: Processing JSON-driven task '{}' with {} input types and {} output types",
                     task_id, task_details.input_types.size(), task_details.output_types.size());

        // Lambda to create and configure handlers for given message types
        auto activate = [&](const std::vector<MessageType>& list, HandlerMode mode){
            for (auto mt : list) {
                auto handler = handler_factory_->get_or_create(mt);
                if (handler) {
                    MessageHandlerBase::configure_handler_mode(handler, mode);
                    gateway_->register_handler(handler);
                    logger_.Info("gateway_controller.cpp: Activated handler for msgType {} with mode {}",
                                  static_cast<int>(mt), static_cast<int>(mode));
                } else {
                    logger_.Error("gateway_controller.cpp: No handler registered for msgType {}", static_cast<int>(mt));
                }
            }
        };

        // VHC: subscribe to inputs (send), publish outputs (receive)
        // MEC: publish inputs (receive), subscribe to outputs (send)
        if (component_type_ == "V") {
            activate(task_details.input_types, HandlerMode::SUBSCRIBER_ONLY);
            activate(task_details.output_types, HandlerMode::PUBLISHER_ONLY);
        } else if (component_type_ == "M") {
            activate(task_details.input_types, HandlerMode::PUBLISHER_ONLY);
            activate(task_details.output_types, HandlerMode::SUBSCRIBER_ONLY);
        } else {
            activate(task_details.input_types, HandlerMode::BOTH);
            activate(task_details.output_types, HandlerMode::BOTH);
        }
    } else {
        // Legacy fallback path
        std::vector<std::pair<std::string, HandlerMode>> handlers_to_create;
        logger_.Info("gateway_controller.cpp: Processing legacy task '{}' with {} required handlers",
                     task_id, task_details.required_handlers.size());
        HandlerMode mode = HandlerMode::BOTH;
        if (component_type_ == "V") mode = HandlerMode::SUBSCRIBER_ONLY; else if (component_type_ == "M") mode = HandlerMode::PUBLISHER_ONLY;
        for (const auto& handler_type : task_details.required_handlers) {
            handlers_to_create.emplace_back(handler_type, mode);
        }
        logger_.Info("gateway_controller.cpp: Activating {} legacy handlers for task '{}' (session {}).",
                     handlers_to_create.size(), task_id, request_id);
        for (const auto& [handler_type, mode2] : handlers_to_create) {
            auto handler = handler_factory_->create_handler(handler_type);
            if (handler) {
                MessageHandlerBase::configure_handler_mode(handler, mode2);
                gateway_->register_handler(handler);
                logger_.Info("gateway_controller.cpp: Activated legacy handler '{}' with mode {}", handler_type, static_cast<int>(mode2));
            } else {
                logger_.Error("gateway_controller.cpp: Failed to create legacy handler for type '{}'", handler_type);
            }
        }
    }
}

// Called by BridgeCpClient when OM denies offloading session
// Cleans up any pending session state
void GatewayController::on_session_denied(const std::string& request_id, const std::string& reason) {
    logger_.Warn("gateway_controller.cpp: Session with request_id '{}' denied by Bridge: {}", request_id, reason);
    handle_session_teardown(request_id);
}

} // namespace gateway