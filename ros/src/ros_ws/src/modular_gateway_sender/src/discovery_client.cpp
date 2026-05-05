#include "modular_gateway_sender/discovery_client.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include <vector>
#include <chrono>

namespace gateway {

// Constructor: Initialize Discovery Service client
// Handles registration to obtain Bridge address and global configuration
DiscoveryClient::DiscoveryClient()
    : logger_(CppLogging::Logger("gateway"))
{
    logger_.Info("discovery_client.cpp: DiscoveryClient constructed.");
}

// Destructor: Ensure clean shutdown of client thread and transport
DiscoveryClient::~DiscoveryClient() {
    logger_.Info("discovery_client.cpp: DiscoveryClient destructed.");
    stop();
}

// Start Discovery Service registration and keepalive process
// Called by controller during DISCOVERING state
// Launches thread that registers, waits for response, then maintains keepalive
bool DiscoveryClient::start(
    const std::string& host,
    int port,
    discovery_protocol::ComponentType component_type,
    const std::string& component_name,
    uint8_t group_id,
    uint8_t id_in_group,
    int data_plane_port,
    DiscoverySuccessCallback success_cb,
    DiscoveryFailureCallback failure_cb)
{
    if (running_) {
        logger_.Warn("discovery_client.cpp: Discovery client already running.");
        return true;
    }

    // Store parameters for registration request
    host_ = host;
    port_ = port;
    component_type_ = component_type;
    component_name_ = component_name;
    group_id_ = group_id;
    id_in_group_ = id_in_group;
    data_plane_port_ = data_plane_port;
    success_cb_ = success_cb;
    failure_cb_ = failure_cb;

    // Launch registration and keepalive thread
    running_ = true;
    registered_ = false;
    client_thread_ = std::thread(&DiscoveryClient::client_thread_func, this);
    return true;
}

// Stop client thread and disconnect from Discovery Service
// Called during shutdown or when re-registration needed
void DiscoveryClient::stop() {
    running_ = false;
    if (client_thread_.joinable()) {
        client_thread_.join();
    }
    if (transport_) {
        transport_->disconnect();
    }
    logger_.Info("discovery_client.cpp: Discovery client stopped.");
}

// Main client thread: register with Discovery Service, then maintain keepalive
// Phase 1: Send REGISTRATION_REQUEST with component identity and DP port
// Phase 2: Wait for REGISTRATION_RESPONSE with Bridge address and global config
// Phase 3: Send periodic KEEPALIVE_PING messages to maintain registration
void DiscoveryClient::client_thread_func() {
    transport_ = std::make_unique<TcpClientTransport>(host_, port_);
    
    // Phase 1 & 2: Registration with polling for WAIT response
    while (running_ && !registered_) {
        if (!transport_->is_connected()) {
            if (!transport_->connect()) {
                logger_.Error("discovery_client.cpp: Failed to connect to DiscoveryService at {}:{}", host_, port_);
                if (failure_cb_) failure_cb_("Could not connect to DiscoveryService.");
                return;
            }
            logger_.Info("discovery_client.cpp: Connected to DiscoveryService. Sending registration for component type '{}'.", 
                         discovery_protocol::component_type_to_string(component_type_));
        }

        // Build registration request
        discovery_protocol::RegistrationRequest request_payload;
        request_payload.componentType = component_type_;
        request_payload.idRequestType = discovery_protocol::IdRequestType::STATIC;
        request_payload.groupId = group_id_;
        request_payload.idInGroup = id_in_group_;
        request_payload.componentName = component_name_;
        request_payload.listenAddress = "0.0.0.0";
        request_payload.listenPort = std::to_string(data_plane_port_);
        request_payload.humanReadableMessage = "registration request from MGW";

        discovery_protocol::Message reg_msg(request_payload);
        std::string encoded_msg;
        if (discovery_protocol::encode_message(reg_msg, encoded_msg) != discovery_protocol::ProtocolStatus::OK) {
            logger_.Error("discovery_client.cpp: Failed to encode registration message.");
            if (failure_cb_) failure_cb_("Failed to encode registration message.");
            return;
        }

        std::vector<uint8_t> reg_data(encoded_msg.begin(), encoded_msg.end());
        transport_->async_send_data(std::move(reg_data));

        // Wait for registration response
        if (transport_->data_available(5000)) {
            std::vector<uint8_t> buffer(8192); // Larger buffer for potentially large configJson
            int bytes_received = transport_->receive_data(buffer.data(), buffer.size() - 1);
            if (bytes_received > 0) {
                buffer[bytes_received] = '\0';
                std::string response_str(reinterpret_cast<char*>(buffer.data()));
                
                discovery_protocol::Message response_msg;
                if (discovery_protocol::decode_message(response_str, response_msg) == discovery_protocol::ProtocolStatus::OK) {
                    if (response_msg.type == discovery_protocol::MessageType::REGISTRATION_RESPONSE) {
                        auto& resp_payload = std::get<discovery_protocol::RegistrationResponse>(response_msg.data);
                        
                        if (resp_payload.responseCode == discovery_protocol::ResponseCode::SUCCESS) {
                            logger_.Info("discovery_client.cpp: Registration successful. Target at {}:{}", 
                                         resp_payload.connectionTargetAddress, resp_payload.connectionTargetPort);
                            try {
                                int bridge_port = std::stoi(resp_payload.connectionTargetPort);
                                registered_ = true;
                                if (success_cb_) {
                                    success_cb_(resp_payload.connectionTargetAddress, bridge_port, resp_payload.configJson);
                                }
                            } catch (const std::exception& e) {
                                logger_.Error("discovery_client.cpp: Invalid port received: '{}'", resp_payload.connectionTargetPort);
                                if (failure_cb_) failure_cb_("Invalid port from DiscoveryService.");
                                return;
                            }
                        } else if (resp_payload.responseCode == discovery_protocol::ResponseCode::WAIT) {
                            logger_.Info("discovery_client.cpp: Received WAIT from DiscoveryService: {}. Retrying in 2s...", 
                                         resp_payload.humanReadableMessage);
                            std::this_thread::sleep_for(std::chrono::seconds(2));
                            continue; // Retry the loop
                        } else {
                            logger_.Error("discovery_client.cpp: Registration denied: {}", resp_payload.humanReadableMessage);
                            if (failure_cb_) failure_cb_("Registration denied: " + resp_payload.humanReadableMessage);
                            return;
                        }
                    }
                }
            }
        } else {
            logger_.Warn("discovery_client.cpp: Timed out waiting for registration response.");
            if (failure_cb_) failure_cb_("Timeout waiting for response.");
            return;
        }
    }

    // Phase 3: Keepalive loop - maintain registration with periodic pings
    // Default interval 30s, can be updated via KEEPALIVE_RESPONSE
    logger_.Info("discovery_client.cpp: Entering keepalive loop (interval {} ms)", keepalive_interval_ms_.count());
    auto last_ping_sent = std::chrono::steady_clock::now();

    while (running_ && registered_) {
        auto now = std::chrono::steady_clock::now();
        int wait_ms = static_cast<int>(keepalive_interval_ms_.count());
        
        // Wait for KEEPALIVE_RESPONSE or timeout
        if (transport_->data_available(wait_ms)) {
            std::vector<uint8_t> buffer(4096);
            int bytes = transport_->receive_data(buffer.data(), buffer.size() - 1);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                std::string msg(reinterpret_cast<char*>(buffer.data()));
                discovery_protocol::Message incoming;
                if (discovery_protocol::decode_message(msg, incoming) == discovery_protocol::ProtocolStatus::OK) {
                    if (incoming.type == discovery_protocol::MessageType::KEEPALIVE_RESPONSE) {
                        auto& pong = std::get<discovery_protocol::KeepaliveResponse>(incoming.data);
                        logger_.Info("discovery_client.cpp: Received keepalive response: '{}' nextIntervalMs={}", pong.humanReadableMessage, pong.nextIntervalMs);
                        // Discovery Service can dynamically adjust keepalive interval
                        if (pong.nextIntervalMs > 0) {
                            keepalive_interval_ms_ = std::chrono::milliseconds(pong.nextIntervalMs);
                        }
                        last_ping_sent = now;  // Reset timer after receiving pong
                    } else {
                        logger_.Info("discovery_client.cpp: Ignoring non-keepalive message type {} during keepalive phase", static_cast<int>(incoming.type));
                    }
                } else {
                    logger_.Warn("discovery_client.cpp: Failed to decode incoming keepalive-phase message ({} bytes)", bytes);
                }
            }
        } else {
            // Timeout reached - send KEEPALIVE_PING
            discovery_protocol::KeepalivePing ping_payload;
            // Format component_id as "group_id.id_in_group" (note: uses dot, not colon)
            std::string component_id = std::to_string(group_id_) + "." + std::to_string(id_in_group_);
            ping_payload.componentId = component_id;
            ping_payload.status = "OK";
            ping_payload.humanReadableMessage = "keepalive ping";
            discovery_protocol::Message ping_msg(ping_payload);
            std::string encoded_ping;
            if (discovery_protocol::encode_message(ping_msg, encoded_ping) == discovery_protocol::ProtocolStatus::OK && !encoded_ping.empty()) {
                std::vector<uint8_t> send_buf(encoded_ping.begin(), encoded_ping.end());
                auto result = transport_->async_send_data(std::move(send_buf));
                if (result == TransportAsyncSendResult::SUCCESS) {
                    logger_.Info("discovery_client.cpp: Sent keepalive ping (interval {} ms)", keepalive_interval_ms_.count());
                } else if (result == TransportAsyncSendResult::NOT_CONNECTED) {
                    logger_.Error("discovery_client.cpp: Transport not connected while sending keepalive. Exiting keepalive loop.");
                    break;
                } else {
                    logger_.Warn("discovery_client.cpp: Failed to enqueue keepalive ping (queue/result code {})", static_cast<int>(result));
                }
            } else {
                logger_.Error("discovery_client.cpp: Failed to encode keepalive ping message");
            }
            last_ping_sent = std::chrono::steady_clock::now();
        }
    }

    logger_.Info("discovery_client.cpp: Exiting keepalive loop (running_={}, registered_={})", running_.load(), registered_.load());
}

} // namespace gateway