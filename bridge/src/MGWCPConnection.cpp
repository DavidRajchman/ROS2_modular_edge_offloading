#include "MGWCPConnection.hpp"
#include "logging/logger.h"
#include <algorithm>

MGWCPConnection::MGWCPConnection(uint32_t connection_id,
                               std::shared_ptr<gateway::TcpServerTransport> server_transport,
                               uint32_t transport_client_id,
                               const std::string& client_ip,
                               std::shared_ptr<SessionManager> session_manager,
                               MessageForwarder message_forwarder,
                               DataPlaneConnectCallback dp_connect_callback)
    : connection_id_(connection_id),
      server_transport_(server_transport),
      transport_client_id_(transport_client_id),
      client_ip_(client_ip),
      state_(MGWCPConnectionState::CONNECTING),
      dp_port_(0),
      running_(false),
      session_manager_(session_manager),
      message_forwarder_(message_forwarder),
      dp_connect_callback_(dp_connect_callback),
      sequence_number_(1)
{
    CppLogging::Logger logger("bridge");
    logger.Info("MGWCPConnection.cpp: Created connection {} for client {}", connection_id_, client_ip_);
}

MGWCPConnection::~MGWCPConnection() {
    CppLogging::Logger logger("bridge");
    logger.Info("MGWCPConnection.cpp: Destroying connection {}", connection_id_);
    stop();
}

void MGWCPConnection::start() {
    CppLogging::Logger logger("bridge");
    
    if (running_.load()) {
        logger.Warn("MGWCPConnection.cpp: Connection {} already running", connection_id_);
        return;
    }
    
    if (!server_transport_ || !server_transport_->is_client_connected(transport_client_id_)) {
        logger.Error("MGWCPConnection.cpp: Server transport or client {} not connected for connection {}", 
                    transport_client_id_, connection_id_);
        return;
    }
    
    running_.store(true);
    connection_thread_ = std::thread(&MGWCPConnection::connection_thread_func, this);
    logger.Info("MGWCPConnection.cpp: Started connection thread for connection {}", connection_id_);
}

void MGWCPConnection::stop() {
    CppLogging::Logger logger("bridge");
    
    running_.store(false);
    
    if (connection_thread_.joinable()) {
        logger.Info("MGWCPConnection.cpp: Stopping connection thread for connection {}", connection_id_);
        connection_thread_.join();
    }
    
    if (server_transport_ && server_transport_->is_client_connected(transport_client_id_)) {
        server_transport_->disconnect_client(transport_client_id_);
    }
    
    // Clean up sessions for this MGWCP
    if (session_manager_ && !component_id_.empty()) {
        size_t cleaned_sessions = session_manager_->cleanup_sessions_for_mgwcp(component_id_);
        if (cleaned_sessions > 0) {
            logger.Info("MGWCPConnection.cpp: Cleaned up {} sessions for disconnected MGWCP '{}'", 
                       cleaned_sessions, component_id_);
        }
    }
    
    logger.Info("MGWCPConnection.cpp: Stopped connection {}", connection_id_);
}

void MGWCPConnection::connection_thread_func() {
    CppLogging::Logger logger("bridge");
    logger.Info("MGWCPConnection.cpp: Connection thread started for connection {}", connection_id_);
    
    std::vector<uint8_t> buffer(MAX_MESSAGE_SIZE);
    
    while (running_.load() && server_transport_ && server_transport_->is_client_connected(transport_client_id_)) {
        // Check for incoming data with timeout
        if (server_transport_->data_available_from(transport_client_id_, 1000)) { // 1 second timeout
            int bytes_received = server_transport_->receive_from(transport_client_id_, buffer.data(), buffer.size() - 1);
            logger.Debug("MGWCPConnection.cpp: READ SOME DATA ");
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                std::string json_str(reinterpret_cast<char*>(buffer.data()), bytes_received);
                
                // LOG ALL INCOMING CP MESSAGES AT INFO LEVEL
                logger.Debug("MGWCPConnection.cpp: [INCOMING CP MESSAGE] Connection {}: {}", connection_id_, json_str);


                try {
                    nlohmann::json message = nlohmann::json::parse(json_str);
                    handle_received_message(message);
                } catch (const nlohmann::json::parse_error& e) {
                    logger.Error("MGWCPConnection.cpp: JSON parse error from connection {}: {}. Data: {}", 
                                connection_id_, e.what(), json_str);
                }
            } else if (bytes_received < 0) {
                logger.Error("MGWCPConnection.cpp: Receive error on connection {}", connection_id_);
                break;
            }
        }
        
        // Check for ACK timeouts and retries
        check_for_timeouts();
    }
    
    logger.Info("MGWCPConnection.cpp: Connection thread finished for connection {}", connection_id_);
}

void MGWCPConnection::handle_received_message(const nlohmann::json& message) {
    CppLogging::Logger logger("bridge");
    
    if (!validate_message_structure(message)) {
        logger.Error("MGWCPConnection.cpp: Invalid message structure from connection {}", connection_id_);
        return;
    }


    
    // Extract component_id and set it if this is the first message
    std::string msg_component_id = message["component_id"];
    if (component_id_.empty()) {
        if (is_valid_component_id(msg_component_id)) {
            component_id_ = msg_component_id;
            logger.Info("MGWCPConnection.cpp: Set component_id '{}' for connection {}", component_id_, connection_id_);
        } else {
            logger.Error("MGWCPConnection.cpp: Invalid component_id '{}' from connection {}", 
                        msg_component_id, connection_id_);
            return;
        }
    } else if (component_id_ != msg_component_id) {
        logger.Error("MGWCPConnection.cpp: Component_id mismatch. Expected '{}', got '{}' from connection {}", 
                    component_id_, msg_component_id, connection_id_);
        return;
    }
    
    int message_code = message["message_code"];
    uint64_t seq_num = message.value("sequence_number", 0);
    
    // Send ACK for all non-ACK messages
    if (message_code != 900) {
        send_ack(seq_num);
    }
    
    // Handle message based on code
    switch (message_code) {
        case 103: // DP_INFO
            handle_dp_info(message["payload"]);
            break;
        case 100: // OFFLOAD_REQUEST
            handle_offload_request(message["payload"]);
            break;
        case 101: // SESSION_KEEPALIVE
            handle_session_keepalive(message["payload"]);
            break;
        case 102: // SESSION_TERMINATE_REQUEST
            handle_session_terminate_request(message["payload"]);
            break;
        case 900: // ACK
            handle_ack(message["payload"]);
            break;
        default:
            logger.Warn("MGWCPConnection.cpp: Unhandled message code {} from connection {}", 
                       message_code, connection_id_);
            break;
    }
}

void MGWCPConnection::handle_dp_info(const nlohmann::json& payload) {
    CppLogging::Logger logger("bridge");
    
    if (state_.load() != MGWCPConnectionState::CONNECTING) {
        logger.Warn("MGWCPConnection.cpp: Received DP_INFO in wrong state from connection {}", connection_id_);
        return;
    }
    
    if (!payload.contains("dp_host") || !payload.contains("dp_port")) {
        logger.Error("MGWCPConnection.cpp: Invalid DP_INFO payload from connection {}", connection_id_);
        return;
    }
    
    std::string requested_host = payload["dp_host"];
    dp_port_ = payload["dp_port"];
    
    // Use the auto-detected IP address instead of the one from the request if it's 0.0.0.0
    // This follows the same pattern as the Discovery Service
    if (requested_host == "0.0.0.0") {
        dp_host_ = client_ip_;  // Use the real client IP detected by transport layer
        logger.Info("MGWCPConnection.cpp: DP_INFO contained 0.0.0.0, using auto-detected client IP: {}", dp_host_);
    } else {
        dp_host_ = requested_host;
    }
    
    logger.Info("MGWCPConnection.cpp: Received DP_INFO from '{}': requested_host={}, using_host={}, port={}", 
               component_id_, requested_host, dp_host_, dp_port_);
    
    // Attempt data plane connection
    if (dp_connect_callback_(component_id_, dp_host_, dp_port_)) {
        set_state(MGWCPConnectionState::OPERATIONAL);
        send_dp_connection_confirmed();
        logger.Info("MGWCPConnection.cpp: Data plane connected for '{}', sent DP_CONNECTION_CONFIRMED", component_id_);
    } else {
        logger.Error("MGWCPConnection.cpp: Failed to establish data plane connection for '{}'", component_id_);
        set_state(MGWCPConnectionState::WAITING_FOR_DP);
    }
}

void MGWCPConnection::handle_offload_request(const nlohmann::json& payload) {
    CppLogging::Logger logger("bridge");
    
    if (state_.load() != MGWCPConnectionState::OPERATIONAL) {
        logger.Warn("MGWCPConnection.cpp: Received OFFLOAD_REQUEST in non-operational state from '{}'", component_id_);
        return;
    }
    
    if (!payload.contains("request_id") || !payload.contains("task_id") || !payload.contains("task_name")) {
        logger.Error("MGWCPConnection.cpp: Invalid OFFLOAD_REQUEST payload from '{}'", component_id_);
        return;
    }
    
    std::string request_id = payload["request_id"];
    uint32_t task_id = payload["task_id"];
    std::string task_name = payload["task_name"];
    
    logger.Info("MGWCPConnection.cpp: Received OFFLOAD_REQUEST from '{}': request_id='{}', task_id={}, task_name='{}'", 
               component_id_, request_id, task_id, task_name);
    
    // Forward to OM via BridgeControlPlane
    nlohmann::json forward_message = {
        {"component_id", component_id_},
        {"message_code", 100},
        {"message_type", "OFFLOAD_REQUEST"},
        {"payload", payload}
    };
    
    message_forwarder_(component_id_, forward_message);
}

void MGWCPConnection::handle_session_keepalive(const nlohmann::json& payload) {
    CppLogging::Logger logger("bridge");
    
    if (!payload.contains("request_id")) {
        logger.Error("MGWCPConnection.cpp: Invalid SESSION_KEEPALIVE payload from '{}'", component_id_);
        return;
    }
    
    std::string request_id = payload["request_id"];
    
    // Update keepalive in session manager
    if (session_manager_->update_keepalive(request_id)) {
        logger.Info("MGWCPConnection.cpp: Updated keepalive for session '{}' from '{}'", request_id, component_id_);
    } else {
        logger.Warn("MGWCPConnection.cpp: Unknown session '{}' in keepalive from '{}'", request_id, component_id_);
    }
    
    // Forward to OM
    nlohmann::json forward_message = {
        {"component_id", component_id_},
        {"message_code", 101},
        {"message_type", "SESSION_KEEPALIVE"},
        {"payload", payload}
    };
    
    message_forwarder_(component_id_, forward_message);
}

void MGWCPConnection::handle_session_terminate_request(const nlohmann::json& payload) {
    CppLogging::Logger logger("bridge");
    
    if (!payload.contains("request_id")) {
        logger.Error("MGWCPConnection.cpp: Invalid SESSION_TERMINATE_REQUEST payload from '{}'", component_id_);
        return;
    }
    
    std::string request_id = payload["request_id"];
    logger.Info("MGWCPConnection.cpp: Received SESSION_TERMINATE_REQUEST for '{}' from '{}'", request_id, component_id_);
    
    // Forward to OM
    nlohmann::json forward_message = {
        {"component_id", component_id_},
        {"message_code", 102},
        {"message_type", "SESSION_TERMINATE_REQUEST"},
        {"payload", payload}
    };
    
    message_forwarder_(component_id_, forward_message);
}

void MGWCPConnection::handle_ack(const nlohmann::json& payload) {
    CppLogging::Logger logger("bridge");
    
    if (!payload.contains("ack_sequence_number")) {
        logger.Error("MGWCPConnection.cpp: Invalid ACK payload from '{}'", component_id_);
        return;
    }
    
    uint64_t ack_seq_num = payload["ack_sequence_number"];
    
    std::lock_guard<std::mutex> lock(pending_acks_mutex_);
    if (pending_acks_.erase(ack_seq_num)) {
        logger.Debug("MGWCPConnection.cpp: Received ACK for sequence {} from '{}'", ack_seq_num, component_id_);
    } else {
        logger.Warn("MGWCPConnection.cpp: Received ACK for unknown sequence {} from '{}'", ack_seq_num, component_id_);
    }
}

bool MGWCPConnection::send_session_approved(const std::string& request_id, const nlohmann::json& payload) {
    CppLogging::Logger logger("bridge");
    
    nlohmann::json message = {
        {"component_id", "15:10"}, // Bridge component ID
        {"message_code", 200},
        {"message_type", "SESSION_APPROVED"},
        {"payload", payload}
    };
    
    send_reliable_message(message);
    logger.Info("MGWCPConnection.cpp: Sent SESSION_APPROVED for '{}' to '{}'", request_id, component_id_);
    return true;
}

bool MGWCPConnection::send_session_denied(const std::string& request_id, const std::string& reason) {
    CppLogging::Logger logger("bridge");
    
    nlohmann::json message = {
        {"component_id", "15:10"}, // Bridge component ID
        {"message_code", 201},
        {"message_type", "SESSION_DENIED"},
        {"payload", {
            {"request_id", request_id},
            {"reason_description", reason}
        }}
    };
    
    send_reliable_message(message);
    logger.Info("MGWCPConnection.cpp: Sent SESSION_DENIED for '{}' to '{}': {}", request_id, component_id_, reason);
    return true;
}

bool MGWCPConnection::send_dp_connection_confirmed() {
    CppLogging::Logger logger("bridge");
    
    nlohmann::json message = {
        {"component_id", "15:10"}, // Bridge component ID
        {"message_code", 202},
        {"message_type", "DP_CONNECTION_CONFIRMED"},
        {"payload", {}}
    };
    
    send_reliable_message(message);
    logger.Info("MGWCPConnection.cpp: Sent DP_CONNECTION_CONFIRMED to '{}'", component_id_);
    return true;
}

void MGWCPConnection::send_reliable_message(nlohmann::json& message) {
    CppLogging::Logger logger("bridge");
    
    if (!server_transport_ || !server_transport_->is_client_connected(transport_client_id_)) {
        logger.Error("MGWCPConnection.cpp: Cannot send message to '{}' - not connected", component_id_);
        return;
    }
    
    uint64_t seq_num = sequence_number_.fetch_add(1);
    message["sequence_number"] = seq_num;
    
    std::string message_str = message.dump();
    std::vector<uint8_t> data(message_str.begin(), message_str.end());
    
    {
        std::lock_guard<std::mutex> lock(pending_acks_mutex_);
        pending_acks_[seq_num] = {message, std::chrono::steady_clock::now(), 0};
    }
    
    if (!server_transport_->send_to(transport_client_id_, data.data(), data.size())) {
        logger.Error("MGWCPConnection.cpp: Failed to send message to '{}'", component_id_);
        std::lock_guard<std::mutex> lock(pending_acks_mutex_);
        pending_acks_.erase(seq_num);
    } else {
        logger.Debug("MGWCPConnection.cpp: Sent reliable message seq={} to '{}'", seq_num, component_id_);
    }
}

void MGWCPConnection::send_ack(uint64_t ack_sequence_number) {
    CppLogging::Logger logger("bridge");
    
    if (!server_transport_ || !server_transport_->is_client_connected(transport_client_id_)) {
        logger.Error("MGWCPConnection.cpp: Cannot send ACK to '{}' - not connected", component_id_);
        return;
    }
    
    nlohmann::json ack_message = {
        {"component_id", component_id_}, // Use the sender's component ID, not Bridge's
        {"message_code", 900},
        {"message_type", "ACK"},
        {"sequence_number", 0}, // ACKs don't need their own sequence number
        {"payload", {
            {"ack_sequence_number", ack_sequence_number}
        }}
    };
    
    std::string message_str = ack_message.dump();
    std::vector<uint8_t> data(message_str.begin(), message_str.end());
    
    if (!server_transport_->send_to(transport_client_id_, data.data(), data.size())) {
        logger.Error("MGWCPConnection.cpp: Failed to send ACK for seq={} to '{}'", ack_sequence_number, component_id_);
    } else {
        logger.Debug("MGWCPConnection.cpp: Sent ACK for seq={} to '{}'", ack_sequence_number, component_id_);
    }
}

void MGWCPConnection::check_for_timeouts() {
    std::lock_guard<std::mutex> lock(pending_acks_mutex_);
    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> to_retry;
    std::vector<uint64_t> to_remove;
    
    for (auto& [seq_num, pending] : pending_acks_) {
        if (now - pending.time_sent > ACK_TIMEOUT_) {
            if (pending.retry_count < MAX_RETRY_COUNT) {
                to_retry.push_back(seq_num);
                pending.retry_count++;
                pending.time_sent = now;
            } else {
                to_remove.push_back(seq_num);
            }
        }
    }
    
    // Remove failed messages
    for (uint64_t seq_num : to_remove) {
        CppLogging::Logger logger("bridge");
        logger.Error("MGWCPConnection.cpp: Message seq={} to '{}' failed after {} retries", 
                    seq_num, component_id_, MAX_RETRY_COUNT);
        pending_acks_.erase(seq_num);
    }
    
    // Retry messages (outside the lock to avoid deadlock)
    if (!to_retry.empty()) {
        // Note: We should unlock the mutex before sending, but the messages are already
        // updated with new time_sent and retry_count, so we can safely retry
        for (uint64_t seq_num : to_retry) {
            auto it = pending_acks_.find(seq_num);
            if (it != pending_acks_.end()) {
                CppLogging::Logger logger("bridge");
                logger.Warn("MGWCPConnection.cpp: Retrying message seq={} to '{}' (attempt {})", 
                           seq_num, component_id_, it->second.retry_count);
                
                std::string message_str = it->second.message.dump();
                std::vector<uint8_t> data(message_str.begin(), message_str.end());
                server_transport_->send_to(transport_client_id_, data.data(), data.size());
            }
        }
    }
}

bool MGWCPConnection::validate_message_structure(const nlohmann::json& message) {
    return message.contains("component_id") && 
           message.contains("message_code") && 
           message.contains("message_type") && 
           message.contains("sequence_number") && 
           message.contains("payload");
}

bool MGWCPConnection::is_valid_component_id(const std::string& component_id) {
    // Component ID should be in format "group_id:id_in_group"
    size_t colon_pos = component_id.find(':');
    if (colon_pos == std::string::npos) {
        return false;
    }
    
    try {
        std::string group_part = component_id.substr(0, colon_pos);
        std::string id_part = component_id.substr(colon_pos + 1);
        
        int group_id = std::stoi(group_part);
        int id_in_group = std::stoi(id_part);
        
        return group_id >= 0 && group_id <= 255 && id_in_group >= 0 && id_in_group <= 255;
    } catch (const std::exception&) {
        return false;
    }
}

void MGWCPConnection::set_state(MGWCPConnectionState new_state) {
    CppLogging::Logger logger("bridge");
    MGWCPConnectionState old_state = state_.exchange(new_state);
    
    if (old_state != new_state) {
        const char* state_names[] = {"CONNECTING", "WAITING_FOR_DP", "OPERATIONAL"};
        logger.Info("MGWCPConnection.cpp: State transition for '{}': {} -> {}", 
                   component_id_, state_names[static_cast<int>(old_state)], state_names[static_cast<int>(new_state)]);
    }
}