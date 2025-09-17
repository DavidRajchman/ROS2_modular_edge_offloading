#include "BridgeControlPlane.hpp"
#include "logging/logger.h"
#include <algorithm>
#include <sstream>

BridgeControlPlane::BridgeControlPlane()
    : global_config_(std::make_shared<GlobalConfig>()),
      session_manager_(std::make_shared<SessionManager>()),
      routing_table_(std::make_shared<RoutingTable>(1000)), // Capacity for 1000 routes
      running_(false),
      shutdown_requested_(false),
      discovery_connected_(false),
      om_connected_(false),
      next_mgwcp_connection_id_(1),
      om_host_(""),  // Will be set from Discovery Service response
      om_port_(0)    // Will be set from Discovery Service response
{
    CppLogging::Logger logger("bridge");
    logger.Info("BridgeControlPlane.cpp: Bridge Control Plane constructed");
}

BridgeControlPlane::~BridgeControlPlane() {
    CppLogging::Logger logger("bridge");
    logger.Info("BridgeControlPlane.cpp: Bridge Control Plane destructor called");
    stop();
}

bool BridgeControlPlane::start() {
    CppLogging::Logger logger("bridge");
    
    if (running_.load()) {
        logger.Warn("BridgeControlPlane.cpp: Bridge CP already running");
        return true;
    }
    
    logger.Info("BridgeControlPlane.cpp: Starting Bridge Control Plane...");
    
    // Phase 1: Register with Discovery Service
    if (!register_with_discovery_service()) {
        logger.Error("BridgeControlPlane.cpp: Failed to register with Discovery Service");
        return false;
    }
    
    // Phase 2: Connect to OM (using address from Discovery Service)
    if (!connect_to_om()) {
        logger.Error("BridgeControlPlane.cpp: Failed to connect to OM");
        return false;
    }
    
    // Phase 3: Start MGWCP server
    if (!start_mgwcp_server()) {
        logger.Error("BridgeControlPlane.cpp: Failed to start MGWCP server");
        return false;
    }
    
    // Phase 4: Start periodic tasks
    start_periodic_tasks();
    
    running_.store(true);
    logger.Info("BridgeControlPlane.cpp: Bridge Control Plane started successfully");
    return true;
}

void BridgeControlPlane::stop() {
    CppLogging::Logger logger("bridge");
    
    if (!running_.load()) {
        return;
    }
    
    logger.Info("BridgeControlPlane.cpp: Stopping Bridge Control Plane...");
    shutdown_requested_.store(true);
    running_.store(false);
    
    // Stop MGWCP server
    if (mgwcp_server_) {
        mgwcp_server_->disconnect();
    }
    
    // Close all MGWCP connections
    {
        std::lock_guard<std::mutex> lock(mgwcp_connections_mutex_);
        for (auto& pair : mgwcp_connections_) {
            pair.second->stop();
        }
        mgwcp_connections_.clear();
        component_id_to_transport_id_.clear();
    }
    
    // Stop transport handlers
    {
        std::lock_guard<std::mutex> lock(transport_handlers_mutex_);
        for (auto& pair : transport_handlers_) {
            pair.second->stop();
        }
        transport_handlers_.clear();
    }
    
    // Disconnect from OM
    if (om_transport_) {
        om_transport_->disconnect();
    }
    
    // Disconnect from Discovery Service
    if (discovery_transport_) {
        discovery_transport_->disconnect();
    }
    
    // Join threads
    if (discovery_keepalive_thread_.joinable()) {
        discovery_keepalive_thread_.join();
    }
    
    if (om_connection_thread_.joinable()) {
        om_connection_thread_.join();
    }
    
    if (mgwcp_server_thread_.joinable()) {
        mgwcp_server_thread_.join();
    }
    
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
    
    logger.Info("BridgeControlPlane.cpp: Bridge Control Plane stopped");
}

bool BridgeControlPlane::register_with_discovery_service() {
    CppLogging::Logger logger("bridge");
    
    logger.Info("BridgeControlPlane.cpp: Registering with Discovery Service at {}:{}", 
               DISCOVERY_SERVICE_HOST, DISCOVERY_SERVICE_PORT);
    
    // Create Discovery Service connection
    discovery_transport_ = std::make_unique<gateway::TcpClientTransport>(
        DISCOVERY_SERVICE_HOST, DISCOVERY_SERVICE_PORT);
    
    if (!discovery_transport_->connect()) {
        logger.Error("BridgeControlPlane.cpp: Failed to connect to Discovery Service");
        return false;
    }
    
    // Perform registration
    if (!perform_discovery_registration()) {
        logger.Error("BridgeControlPlane.cpp: Discovery Service registration failed");
        return false;
    }
    
    discovery_connected_.store(true);
    
    // Start keepalive thread
    discovery_keepalive_thread_ = std::thread(&BridgeControlPlane::discovery_keepalive_thread_func, this);
    
    logger.Info("BridgeControlPlane.cpp: Successfully registered with Discovery Service");
    return true;
}

bool BridgeControlPlane::perform_discovery_registration() {
    CppLogging::Logger logger("bridge");
    
    // Create registration request
    discovery_protocol::RegistrationRequest request;
    request.componentType = discovery_protocol::ComponentType::BRIDGE;
    request.idRequestType = discovery_protocol::IdRequestType::STATIC;
    request.groupId = 5;  // Bridge group
    request.idInGroup = 1;
    request.componentName = "BridgeControlPlane";
    request.listenAddress = "0.0.0.0";  // Discovery service will detect actual IP
    request.listenPort = std::to_string(MGWCP_SERVER_PORT);
    request.humanReadableMessage = "Bridge Control Plane requesting registration";
    
    // Encode message once (it doesn't change between retries)
    discovery_protocol::Message message(request);
    std::string encoded_message;
    if (discovery_protocol::encode_message(message, encoded_message) != discovery_protocol::ProtocolStatus::OK) {
        logger.Error("BridgeControlPlane.cpp: Failed to encode Discovery Service registration request");
        return false;
    }
    
    // Declare response variable outside loop so it's accessible after loop ends
    discovery_protocol::RegistrationResponse response;
    
    // Retry loop for registration
    while (true) {
        // Send registration request
        if (!discovery_transport_->send_data(encoded_message.data(), encoded_message.size())) {
            logger.Error("BridgeControlPlane.cpp: Failed to send Discovery Service registration request");
            return false;
        }
        
        logger.Info("BridgeControlPlane.cpp: Sent Discovery Service registration request");
        
        // Wait for response
        std::vector<uint8_t> response_buffer(2048);
        int bytes_received = discovery_transport_->receive_data(response_buffer.data(), response_buffer.size());
        
        if (bytes_received <= 0) {
            logger.Error("BridgeControlPlane.cpp: Failed to receive Discovery Service registration response");
            return false;
        }
        
        // Decode response
        std::string response_str(response_buffer.begin(), response_buffer.begin() + bytes_received);
        discovery_protocol::Message response_message;
        
        if (discovery_protocol::decode_message(response_str, response_message) != discovery_protocol::ProtocolStatus::OK) {
            logger.Error("BridgeControlPlane.cpp: Failed to decode Discovery Service registration response");
            return false;
        }
        
        if (response_message.type != discovery_protocol::MessageType::REGISTRATION_RESPONSE) {
            logger.Error("BridgeControlPlane.cpp: Unexpected Discovery Service response message type");
            return false;
        }
        
        response = std::get<discovery_protocol::RegistrationResponse>(response_message.data);
        
        if (response.responseCode == discovery_protocol::ResponseCode::SUCCESS) {
            // Success - break out of retry loop and continue with normal processing
            logger.Info("BridgeControlPlane.cpp: Discovery Service registration successful");
            break;
        } else if (response.responseCode == discovery_protocol::ResponseCode::WAIT) {
            // Expected response when OM is not ready yet - wait and retry
            logger.Info("BridgeControlPlane.cpp: Discovery Service responded with WAIT (OM not ready), retrying in 1 second...");
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        } else {
            // Other error codes are actual failures
            logger.Error("BridgeControlPlane.cpp: Discovery Service registration failed: {}", 
                        response.humanReadableMessage);
            return false;
        }
    }
    
    // Extract OM connection details from Discovery Service response
    if (!response.connectionTargetAddress.empty() && !response.connectionTargetPort.empty()) {
        om_host_ = response.connectionTargetAddress;
        try {
            om_port_ = static_cast<uint16_t>(std::stoi(response.connectionTargetPort));
            logger.Info("BridgeControlPlane.cpp: OM connection target received: {}:{}", om_host_, om_port_);
        } catch (const std::exception& e) {
            logger.Error("BridgeControlPlane.cpp: Invalid OM port in Discovery Service response: {}", 
                        response.connectionTargetPort);
            return false;
        }
    } else {
        logger.Error("BridgeControlPlane.cpp: No OM connection target provided by Discovery Service");
        return false;
    }
    
    // Parse global configuration
    if (!response.configJson.empty()) {
        try {
            nlohmann::json config_json = nlohmann::json::parse(response.configJson);
            if (!global_config_->parse_from_json(config_json)) {
                logger.Error("BridgeControlPlane.cpp: Failed to parse global configuration from Discovery Service");
                return false;
            }
            logger.Info("BridgeControlPlane.cpp: Global configuration loaded successfully");
        } catch (const std::exception& e) {
            logger.Error("BridgeControlPlane.cpp: Failed to parse global configuration JSON: {}", e.what());
            return false;
        }
    } else {
        logger.Warn("BridgeControlPlane.cpp: No global configuration received from Discovery Service");
    }
    
    logger.Info("BridgeControlPlane.cpp: Discovery Service registration successful. Assigned ID: {}:{}", 
               response.assignedGroupId, response.assignedIdInGroup);
    
    return true;
}

void BridgeControlPlane::discovery_keepalive_thread_func() {
    CppLogging::Logger logger("bridge");
    logger.Info("BridgeControlPlane.cpp: Discovery Service keepalive thread started");
    
    while (!shutdown_requested_.load() && discovery_connected_.load()) {
        std::this_thread::sleep_for(DISCOVERY_KEEPALIVE_INTERVAL_);
        
        if (shutdown_requested_.load()) {
            break;
        }
        
        // Send keepalive ping
        discovery_protocol::KeepalivePing ping;
        ping.componentId = "5.1";  // Bridge component ID
        ping.status = "OK";
        ping.humanReadableMessage = "Bridge CP keepalive";
        
        discovery_protocol::Message message(ping);
        std::string encoded_message;
        
        if (discovery_protocol::encode_message(message, encoded_message) == discovery_protocol::ProtocolStatus::OK) {
            if (discovery_transport_->send_data(encoded_message.data(), encoded_message.size())) {
                last_discovery_keepalive_ = std::chrono::steady_clock::now();
                logger.Debug("BridgeControlPlane.cpp: Sent Discovery Service keepalive");
            } else {
                logger.Warn("BridgeControlPlane.cpp: Failed to send Discovery Service keepalive");
                discovery_connected_.store(false);
                break;
            }
        }
    }
    
    logger.Info("BridgeControlPlane.cpp: Discovery Service keepalive thread stopped");
}

bool BridgeControlPlane::connect_to_om() {
    CppLogging::Logger logger("bridge");
    
    // Verify OM connection details were received from Discovery Service
    if (om_host_.empty() || om_port_ == 0) {
        logger.Error("BridgeControlPlane.cpp: No OM connection details available from Discovery Service");
        return false;
    }
    
    logger.Info("BridgeControlPlane.cpp: Connecting to OM at {}:{}", om_host_, om_port_);
    
    // Create OM connection using details from Discovery Service
    om_transport_ = std::make_unique<gateway::TcpClientTransport>(om_host_, om_port_);
    
    if (!om_transport_->connect()) {
        logger.Error("BridgeControlPlane.cpp: Failed to connect to OM at {}:{}", om_host_, om_port_);
        return false;
    }
    
    om_connected_.store(true);
    
    // Start OM connection thread
    om_connection_thread_ = std::thread(&BridgeControlPlane::om_connection_thread_func, this);
    
    logger.Info("BridgeControlPlane.cpp: Successfully connected to OM at {}:{}", om_host_, om_port_);
    return true;
}

void BridgeControlPlane::om_connection_thread_func() {
    CppLogging::Logger logger("bridge");
    logger.Info("BridgeControlPlane.cpp: OM connection thread started");
    
    std::vector<uint8_t> buffer(8192);
    
    while (!shutdown_requested_.load() && om_connected_.load()) {
        if (om_transport_->data_available(1000)) {  // 1 second timeout
            int bytes_received = om_transport_->receive_data(buffer.data(), buffer.size());
            
            if (bytes_received > 0) {
                std::string message_str(buffer.begin(), buffer.begin() + bytes_received);
                
                try {
                    nlohmann::json message = nlohmann::json::parse(message_str);
                    handle_om_message(message);
                } catch (const nlohmann::json::parse_error& e) {
                    logger.Error("BridgeControlPlane.cpp: Failed to parse OM message JSON: {}", e.what());
                }
            } else if (bytes_received < 0) {
                logger.Error("BridgeControlPlane.cpp: OM connection error");
                om_connected_.store(false);
                break;
            }
        }
    }
    
    logger.Info("BridgeControlPlane.cpp: OM connection thread stopped");
}

void BridgeControlPlane::handle_om_message(const nlohmann::json& message) {
    CppLogging::Logger logger("bridge");
    
    if (!message.contains("message_code") || !message.contains("payload")) {
        logger.Error("BridgeControlPlane.cpp: Invalid OM message structure");
        return;
    }
    
    int message_code = message["message_code"];
    logger.Debug("BridgeControlPlane.cpp: Received OM message with code {}", message_code);
    
    switch (message_code) {
        case 200: // SESSION_APPROVED
            handle_session_approved(message);
            break;
        case 201: // SESSION_DENIED
            handle_session_denied(message);
            break;
        default:
            logger.Warn("BridgeControlPlane.cpp: Unhandled OM message code {}", message_code);
            break;
    }
}

void BridgeControlPlane::send_om_message(const nlohmann::json& message) {
    CppLogging::Logger logger("bridge");
    
    if (!om_connected_.load() || !om_transport_) {
        logger.Error("BridgeControlPlane.cpp: Cannot send OM message - not connected");
        return;
    }
    
    std::string message_str = message.dump();
    
    {
        std::lock_guard<std::mutex> lock(om_send_mutex_);
        if (!om_transport_->send_data(message_str.data(), message_str.size())) {
            logger.Error("BridgeControlPlane.cpp: Failed to send message to OM");
            om_connected_.store(false);
        } else {
            logger.Debug("BridgeControlPlane.cpp: Sent message to OM: {}", message_str);
        }
    }
}

bool BridgeControlPlane::start_mgwcp_server() {
    CppLogging::Logger logger("bridge");
    
    logger.Info("BridgeControlPlane.cpp: Starting MGWCP server on port {}", MGWCP_SERVER_PORT);
    
    // Create MGWCP server as shared_ptr
    mgwcp_server_ = std::make_shared<gateway::TcpServerTransport>(MGWCP_SERVER_PORT, true, MAX_MGWCP_CONNECTIONS);
    
    // Set up callbacks
    mgwcp_server_->set_connect_callback([this](uint32_t client_id, const std::string& ip, int port) {
        handle_new_mgwcp_connection(client_id, ip);
    });
    
    mgwcp_server_->set_disconnect_callback([this](uint32_t client_id) {
        handle_mgwcp_disconnection(client_id);
    });
    
    if (!mgwcp_server_->connect()) {
        logger.Error("BridgeControlPlane.cpp: Failed to start MGWCP server");
        return false;
    }
    
    // Start server thread
    mgwcp_server_thread_ = std::thread(&BridgeControlPlane::mgwcp_server_thread_func, this);
    
    logger.Info("BridgeControlPlane.cpp: MGWCP server started successfully");
    return true;
}

void BridgeControlPlane::mgwcp_server_thread_func() {
    CppLogging::Logger logger("bridge");
    logger.Info("BridgeControlPlane.cpp: MGWCP server thread started");
    
    while (!shutdown_requested_.load() && mgwcp_server_ && mgwcp_server_->is_connected()) {
        mgwcp_server_->process_events(1000);  // 1 second timeout
    }
    
    logger.Info("BridgeControlPlane.cpp: MGWCP server thread stopped");
}

void BridgeControlPlane::handle_new_mgwcp_connection(uint32_t transport_client_id, const std::string& client_ip) {
    CppLogging::Logger logger("bridge");
    
    logger.Info("BridgeControlPlane.cpp: New MGWCP connection from {} (transport ID: {})", 
               client_ip, transport_client_id);
    
    // Now we can pass the shared_ptr directly without fake conversion
    auto mgwcp_connection = std::make_unique<MGWCPConnection>(
        next_mgwcp_connection_id_++,
        mgwcp_server_,  // Pass shared_ptr directly
        transport_client_id,
        client_ip,
        session_manager_,
        [this](const std::string& mgwcp_id, const nlohmann::json& msg) {
            forward_message_to_om(mgwcp_id, msg);
        },
        [this](const std::string& mgwcp_id, const std::string& host, int port) {
            return establish_mgwcp_data_plane_connection(mgwcp_id, host, port);
        },
        // component registration callback
        [this](const std::string& component_id, uint32_t transport_id) {
            CppLogging::Logger logger("bridge");
            std::lock_guard<std::mutex> lock(mgwcp_connections_mutex_);
            component_id_to_transport_id_[component_id] = transport_id;
            logger.Info("BridgeControlPlane.cpp: Registered component_id '{}' to transport id {}", component_id, transport_id);
        }
    );
    
    uint32_t connection_id = mgwcp_connection->get_connection_id();
    
    // Store connection
    {
        std::lock_guard<std::mutex> lock(mgwcp_connections_mutex_);
        mgwcp_connections_[transport_client_id] = std::move(mgwcp_connection);
        
        // Start the connection thread to process messages
        mgwcp_connections_[transport_client_id]->start();
    }
    
    logger.Info("BridgeControlPlane.cpp: Created and started MGWCPConnection {} for transport client {}", 
               connection_id, transport_client_id);
}

void BridgeControlPlane::handle_mgwcp_disconnection(uint32_t transport_client_id) {
    CppLogging::Logger logger("bridge");
    
    logger.Info("BridgeControlPlane.cpp: MGWCP disconnection (transport ID: {})", transport_client_id);
    
    std::string component_id;
    
    // Remove connection
    {
        std::lock_guard<std::mutex> lock(mgwcp_connections_mutex_);
        auto it = mgwcp_connections_.find(transport_client_id);
        if (it != mgwcp_connections_.end()) {
            component_id = it->second->get_component_id();
            it->second->stop();
            mgwcp_connections_.erase(it);
            
            // Remove from component ID mapping
            if (!component_id.empty()) {
                component_id_to_transport_id_.erase(component_id);
            }
        }
    }
    
    if (!component_id.empty()) {
        logger.Info("BridgeControlPlane.cpp: Cleaned up MGWCP connection for component '{}'", component_id);
    }
}

void BridgeControlPlane::forward_message_to_om(const std::string& mgwcp_component_id, const nlohmann::json& message) {
    CppLogging::Logger logger("bridge");
    
    logger.Debug("BridgeControlPlane.cpp: Forwarding message from '{}' to OM", mgwcp_component_id);
    
    // Create session if this is an OFFLOAD_REQUEST
    if (message.contains("message_code") && message["message_code"] == 100) {
        const auto& payload = message["payload"];
        if (payload.contains("request_id") && payload.contains("task_id") && payload.contains("task_name")) {
            std::string request_id = payload["request_id"];
            uint32_t task_id = payload["task_id"];
            std::string task_name = payload["task_name"];
            
            if (session_manager_->create_session(request_id, mgwcp_component_id, task_id, task_name, *global_config_)) {
                logger.Info("BridgeControlPlane.cpp: Created session '{}' for MGWCP '{}'", request_id, mgwcp_component_id);
            }
        }
    }
    
    // Forward message to OM WITHOUT overwriting original component_id (bug fix)
    // Preserve the sender's component_id so OM sees the true source (e.g., '60:5').
    // Add bridge identity separately if OM needs to know which bridge forwarded it.
    nlohmann::json forward_message = message;
    if (!forward_message.contains("bridge_component_id")) {
        forward_message["bridge_component_id"] = BRIDGE_COMPONENT_ID; // metadata
    }
    // Remove any accidental previous overwrite (not needed, just ensuring clarity)
    // (Do NOT set forward_message["component_id"] = BRIDGE_COMPONENT_ID;)

    logger.Debug("BridgeControlPlane.cpp: Forward payload to OM (source component_id='{}', bridge_component_id='{}')", 
                 forward_message.value("component_id", "<missing>"), 
                 forward_message.value("bridge_component_id", "<missing>"));
    
    send_om_message(forward_message);
}

bool BridgeControlPlane::establish_mgwcp_data_plane_connection(const std::string& mgwcp_component_id, 
                                                              const std::string& dp_host, int dp_port) {
    CppLogging::Logger logger("bridge");
    
    logger.Info("BridgeControlPlane.cpp: Establishing data plane connection to MGWCP '{}' at {}:{}", 
               mgwcp_component_id, dp_host, dp_port);
    
    return create_transport_handler_for_mgwdp(mgwcp_component_id, dp_host, dp_port);
}

void BridgeControlPlane::handle_session_approved(const nlohmann::json& om_response) {
    CppLogging::Logger logger("bridge");
    
    if (!om_response.contains("payload") || !om_response["payload"].contains("request_id")) {
        logger.Error("BridgeControlPlane.cpp: Invalid SESSION_APPROVED message from OM");
        return;
    }
    
    const auto& payload = om_response["payload"];
    std::string request_id;
    try {
        if (payload["request_id"].is_string()) {
            request_id = payload["request_id"].get<std::string>();
        } else if (payload["request_id"].is_number_integer()) {
            request_id = std::to_string(payload["request_id"].get<long long>());
            logger.Debug("BridgeControlPlane.cpp: Coerced numeric request_id to string '{}'", request_id);
        } else {
            logger.Error("BridgeControlPlane.cpp: Unsupported request_id type in SESSION_APPROVED payload");
            return;
        }
    } catch (const std::exception& e) {
        logger.Error("BridgeControlPlane.cpp: Exception extracting request_id: {}", e.what());
        return;
    }
    
    logger.Info("BridgeControlPlane.cpp: Received SESSION_APPROVED for request '{}'", request_id);
    
    // Update session with MEC assignment if provided
    if (payload.contains("assigned_mec_id") && payload["assigned_mec_id"].is_string()) {
        std::string mec_id = payload["assigned_mec_id"].get<std::string>();
        session_manager_->set_assigned_mec(request_id, mec_id);
        logger.Info("BridgeControlPlane.cpp: Session '{}' assigned to MEC '{}'", request_id, mec_id);
    }
    
    // Create routing rules
    create_routing_rules_for_session(request_id);
    
    // Forward SESSION_APPROVED to VHC
    const ActiveSession* session = session_manager_->get_session(request_id);
    if (session) {
        std::lock_guard<std::mutex> lock(mgwcp_connections_mutex_);
        auto it = component_id_to_transport_id_.find(session->mgwcp_component_id);
        if (it != component_id_to_transport_id_.end()) {
            auto conn_it = mgwcp_connections_.find(it->second);
            if (conn_it != mgwcp_connections_.end()) {
                conn_it->second->send_session_approved(request_id, payload);
                logger.Info("BridgeControlPlane.cpp: Forwarded SESSION_APPROVED to VHC '{}'", session->mgwcp_component_id);
            }
        }
        
        // Forward SESSION_APPROVED to MEC (so MEC knows which task to handle)
        if (!session->assigned_mec_id.empty()) {
            auto mec_it = component_id_to_transport_id_.find(session->assigned_mec_id);
            if (mec_it != component_id_to_transport_id_.end()) {
                auto mec_conn_it = mgwcp_connections_.find(mec_it->second);
                if (mec_conn_it != mgwcp_connections_.end()) {
                    mec_conn_it->second->send_session_approved(request_id, payload);
                    logger.Info("BridgeControlPlane.cpp: Forwarded SESSION_APPROVED to MEC '{}'", session->assigned_mec_id);
                } else {
                    logger.Warn("BridgeControlPlane.cpp: MEC '{}' not found in MGWCP connections - may need transport handler", session->assigned_mec_id);
                }
            } else {
                logger.Warn("BridgeControlPlane.cpp: MEC '{}' component ID not mapped to transport - may need transport handler", session->assigned_mec_id);
            }
        }
    } else {
        logger.Warn("BridgeControlPlane.cpp: SESSION_APPROVED received for unknown session '{}' (may have expired)", request_id);
    }
}

void BridgeControlPlane::handle_session_denied(const nlohmann::json& om_response) {
    CppLogging::Logger logger("bridge");
    
    if (!om_response.contains("payload") || !om_response["payload"].contains("request_id")) {
        logger.Error("BridgeControlPlane.cpp: Invalid SESSION_DENIED message from OM");
        return;
    }
    
    const auto& payload = om_response["payload"];
    std::string request_id;
    try {
        if (payload["request_id"].is_string()) {
            request_id = payload["request_id"].get<std::string>();
        } else if (payload["request_id"].is_number_integer()) {
            request_id = std::to_string(payload["request_id"].get<long long>());
            logger.Debug("BridgeControlPlane.cpp: Coerced numeric request_id to string '{}'", request_id);
        } else {
            logger.Error("BridgeControlPlane.cpp: Unsupported request_id type in SESSION_DENIED payload");
            return;
        }
    } catch (const std::exception& e) {
        logger.Error("BridgeControlPlane.cpp: Exception extracting request_id in SESSION_DENIED: {}", e.what());
        return;
    }
    
    std::string reason = payload.value("reason_description", std::string("No reason provided"));
    logger.Info("BridgeControlPlane.cpp: Received SESSION_DENIED for request '{}': {}", request_id, reason);
    
    // Remove session (only if it exists)
    session_manager_->remove_session(request_id);
    
    // Forward response to appropriate MGWCP (if still present)
    const ActiveSession* session = session_manager_->get_session(request_id);
    if (session) {
        std::lock_guard<std::mutex> lock(mgwcp_connections_mutex_);
        auto it = component_id_to_transport_id_.find(session->mgwcp_component_id);
        if (it != component_id_to_transport_id_.end()) {
            auto conn_it = mgwcp_connections_.find(it->second);
            if (conn_it != mgwcp_connections_.end()) {
                conn_it->second->send_session_denied(request_id, reason);
            }
        }
    }
}

void BridgeControlPlane::create_routing_rules_for_session(const std::string& request_id) {
    CppLogging::Logger logger("bridge");
    
    const ActiveSession* session = session_manager_->get_session(request_id);
    if (!session) {
        logger.Error("BridgeControlPlane.cpp: Cannot create routing rules - session '{}' not found", request_id);
        return;
    }
    
    logger.Info("BridgeControlPlane.cpp: Creating routing rules for session '{}'", request_id);
    
    // Parse MGWCP component ID to extract group and ID
    std::string mgwcp_id = session->mgwcp_component_id;
    size_t colon_pos = mgwcp_id.find(':');
    if (colon_pos == std::string::npos) {
        logger.Error("BridgeControlPlane.cpp: Invalid MGWCP component ID format: {}", mgwcp_id);
        return;
    }
    
    try {
        uint8_t source_group = static_cast<uint8_t>(std::stoi(mgwcp_id.substr(0, colon_pos)));
        uint8_t source_id = static_cast<uint8_t>(std::stoi(mgwcp_id.substr(colon_pos + 1)));
        
        // Get transport handler queues
        std::shared_ptr<TransportHandler> mgwcp_handler;
        std::shared_ptr<TransportHandler> mec_handler;
        
        {
            std::lock_guard<std::mutex> lock(transport_handlers_mutex_);
            auto mgwcp_it = transport_handlers_.find(session->mgwcp_component_id);
            auto mec_it = transport_handlers_.find(session->assigned_mec_id);
            
            if (mgwcp_it != transport_handlers_.end()) {
                mgwcp_handler = mgwcp_it->second;
            }
            if (mec_it != transport_handlers_.end()) {
                mec_handler = mec_it->second;
            }
        }
        
        if (!mgwcp_handler || !mec_handler) {
            logger.Error("BridgeControlPlane.cpp: Missing transport handlers for routing rules");
            return;
        }
        
        // Create routing rules for input message types (VHC → MEC)
        for (uint8_t msg_type : session->input_message_types) {
            RoutingKey key = construct_routing_key(source_group, source_id, msg_type);
            routing_table_->add_route(key, mec_handler->get_input_queue());
            logger.Info("BridgeControlPlane.cpp: Added VHC→MEC route for {}:{} msg_type {}", (int)source_group, (int)source_id, (int)msg_type);
        }

        // Create routing rules for output message types (MEC → VHC)
        // Parse MEC component ID
        if (!session->assigned_mec_id.empty()) {
            size_t mec_colon = session->assigned_mec_id.find(':');
            if (mec_colon == std::string::npos) {
                logger.Error("BridgeControlPlane.cpp: Invalid MEC component ID format: {}", session->assigned_mec_id);
                return;
            }
            uint8_t mec_group = static_cast<uint8_t>(std::stoi(session->assigned_mec_id.substr(0, mec_colon)));
            uint8_t mec_id = static_cast<uint8_t>(std::stoi(session->assigned_mec_id.substr(mec_colon + 1)));

            for (uint8_t msg_type : session->output_message_types) {
                RoutingKey key = construct_routing_key(mec_group, mec_id, msg_type);
                routing_table_->add_route(key, mgwcp_handler->get_input_queue());
                logger.Info("BridgeControlPlane.cpp: Added MEC→VHC route for {}:{} msg_type {}", (int)mec_group, (int)mec_id, (int)msg_type);
            }
        } else {
            logger.Warn("BridgeControlPlane.cpp: No assigned MEC for session '{}' when creating reverse routes", request_id);
        }
        
        session_manager_->set_routing_rules_created(request_id, true);
        logger.Info("BridgeControlPlane.cpp: Successfully created routing rules for session '{}'", request_id);
        
    } catch (const std::exception& e) {
        logger.Error("BridgeControlPlane.cpp: Failed to parse component ID '{}': {}", mgwcp_id, e.what());
    }
}

void BridgeControlPlane::remove_routing_rules_for_session(const std::string& request_id) {
    CppLogging::Logger logger("bridge");
    
    const ActiveSession* session = session_manager_->get_session(request_id);
    if (!session) {
        logger.Error("BridgeControlPlane.cpp: Cannot remove routing rules - session '{}' not found", request_id);
        return;
    }
    
    logger.Info("BridgeControlPlane.cpp: Removing routing rules for session '{}'", request_id);
    
    // Parse MGWCP component ID
    std::string mgwcp_id = session->mgwcp_component_id;
    size_t colon_pos = mgwcp_id.find(':');
    if (colon_pos == std::string::npos) {
        logger.Error("BridgeControlPlane.cpp: Invalid MGWCP component ID format: {}", mgwcp_id);
        return;
    }
    
    try {
        uint8_t source_group = static_cast<uint8_t>(std::stoi(mgwcp_id.substr(0, colon_pos)));
        uint8_t source_id = static_cast<uint8_t>(std::stoi(mgwcp_id.substr(colon_pos + 1)));
        
        // Remove routing rules for input message types (VHC → MEC)
        for (uint8_t msg_type : session->input_message_types) {
            RoutingKey key = construct_routing_key(source_group, source_id, msg_type);
            routing_table_->remove_routes_for_key(key);
            logger.Info("BridgeControlPlane.cpp: Removed VHC→MEC route for {}:{} msg_type {}", (int)source_group, (int)source_id, (int)msg_type);
        }

        // Remove routing rules for output message types (MEC → VHC)
        if (!session->assigned_mec_id.empty()) {
            size_t mec_colon = session->assigned_mec_id.find(':');
            if (mec_colon != std::string::npos) {
                uint8_t mec_group = static_cast<uint8_t>(std::stoi(session->assigned_mec_id.substr(0, mec_colon)));
                uint8_t mec_id = static_cast<uint8_t>(std::stoi(session->assigned_mec_id.substr(mec_colon + 1)));
                for (uint8_t msg_type : session->output_message_types) {
                    RoutingKey key = construct_routing_key(mec_group, mec_id, msg_type);
                    routing_table_->remove_routes_for_key(key);
                    logger.Info("BridgeControlPlane.cpp: Removed MEC→VHC route for {}:{} msg_type {}", (int)mec_group, (int)mec_id, (int)msg_type);
                }
            } else {
                logger.Error("BridgeControlPlane.cpp: Invalid MEC component ID format during route removal: {}", session->assigned_mec_id);
            }
        }
        
        session_manager_->set_routing_rules_created(request_id, false);
        logger.Info("BridgeControlPlane.cpp: Successfully removed routing rules for session '{}'", request_id);
        
    } catch (const std::exception& e) {
        logger.Error("BridgeControlPlane.cpp: Failed to parse component ID '{}': {}", mgwcp_id, e.what());
    }
}

bool BridgeControlPlane::create_transport_handler_for_mgwdp(const std::string& mgwcp_component_id, 
                                                           const std::string& host, int port) {
    CppLogging::Logger logger("bridge");
    
    std::lock_guard<std::mutex> lock(transport_handlers_mutex_);
    
    // Check if handler already exists
    if (transport_handlers_.find(mgwcp_component_id) != transport_handlers_.end()) {
        logger.Info("BridgeControlPlane.cpp: Transport handler for MGWCP '{}' already exists", mgwcp_component_id);
        return true;
    }
    
    // Create MPSC queue for this handler
    auto input_queue = std::make_shared<MPSCQueueType>();
    
    // Create transport handler
    auto handler = std::make_shared<TransportHandler>(
        mgwcp_component_id,
        host,
        port,
        input_queue,
        routing_table_,
        std::weak_ptr<ITransportHandlerObserver>()  // No observer for now
    );
    
    // Start handler
    handler->start();
    
    // Store handler
    transport_handlers_[mgwcp_component_id] = handler;
    
    logger.Info("BridgeControlPlane.cpp: Created and started transport handler for MGWCP '{}' at {}:{}", 
               mgwcp_component_id, host, port);
    
    return true;
}

bool BridgeControlPlane::create_transport_handler_for_mec(const std::string& mec_component_id, 
                                                         const std::string& host, int port) {
    CppLogging::Logger logger("bridge");
    
    std::lock_guard<std::mutex> lock(transport_handlers_mutex_);
    
    // Check if handler already exists
    if (transport_handlers_.find(mec_component_id) != transport_handlers_.end()) {
        logger.Info("BridgeControlPlane.cpp: Transport handler for MEC '{}' already exists", mec_component_id);
        return true;
    }
    
    // Create MPSC queue for this handler
    auto input_queue = std::make_shared<MPSCQueueType>();
    
    // Create transport handler
    auto handler = std::make_shared<TransportHandler>(
        mec_component_id,
        host,
        port,
        input_queue,
        routing_table_,
        std::weak_ptr<ITransportHandlerObserver>()  // No observer for now
    );
    
    // Start handler
    handler->start();
    
    // Store handler
    transport_handlers_[mec_component_id] = handler;
    
    logger.Info("BridgeControlPlane.cpp: Created and started transport handler for MEC '{}' at {}:{}", 
               mec_component_id, host, port);
    
    return true;
}

void BridgeControlPlane::start_periodic_tasks() {
    CppLogging::Logger logger("bridge");
    
    // Start maintenance thread
    maintenance_thread_ = std::thread(&BridgeControlPlane::maintenance_thread_func, this);
    
    logger.Info("BridgeControlPlane.cpp: Started periodic maintenance tasks");
}

void BridgeControlPlane::maintenance_thread_func() {
    CppLogging::Logger logger("bridge");
    logger.Info("BridgeControlPlane.cpp: Maintenance thread started");
    
    while (!shutdown_requested_.load()) {
        std::this_thread::sleep_for(MAINTENANCE_INTERVAL_);
        
        if (shutdown_requested_.load()) {
            break;
        }
        
        // Check session timeouts
        check_session_timeouts();
        
        // Cleanup expired sessions
        cleanup_expired_sessions();
    }
    
    logger.Info("BridgeControlPlane.cpp: Maintenance thread stopped");
}

void BridgeControlPlane::check_session_timeouts() {
    std::vector<std::string> expired_sessions = session_manager_->find_expired_sessions(SESSION_TIMEOUT_);
    
    if (!expired_sessions.empty()) {
        CppLogging::Logger logger("bridge");
        logger.Info("BridgeControlPlane.cpp: Found {} expired sessions", expired_sessions.size());
        
        for (const std::string& request_id : expired_sessions) {
            logger.Info("BridgeControlPlane.cpp: Session '{}' has expired", request_id);
            
            // Remove routing rules
            remove_routing_rules_for_session(request_id);
            
            // Remove session
            session_manager_->remove_session(request_id);
        }
    }
}

void BridgeControlPlane::cleanup_expired_sessions() {
    // Additional cleanup logic if needed
    // This is separate from timeout checking to allow for different cleanup strategies
}

size_t BridgeControlPlane::get_mgwcp_connection_count() const {
    std::lock_guard<std::mutex> lock(mgwcp_connections_mutex_);
    return mgwcp_connections_.size();
}