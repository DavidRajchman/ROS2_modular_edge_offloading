#include "modular_gateway_sender/discovery_client.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include <vector>
#include <chrono>

namespace gateway {

DiscoveryClient::DiscoveryClient()
    : logger_(CppLogging::Logger("gateway"))
{
    logger_.Info("discovery_client.cpp: DiscoveryClient constructed.");
}

DiscoveryClient::~DiscoveryClient() {
    logger_.Info("discovery_client.cpp: DiscoveryClient destructed.");
    stop();
}

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

    host_ = host;
    port_ = port;
    component_type_ = component_type;
    component_name_ = component_name;
    group_id_ = group_id;
    id_in_group_ = id_in_group;
    data_plane_port_ = data_plane_port;
    success_cb_ = success_cb;
    failure_cb_ = failure_cb;

    running_ = true;
    registered_ = false;
    client_thread_ = std::thread(&DiscoveryClient::client_thread_func, this);
    return true;
}

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

void DiscoveryClient::client_thread_func() {
    transport_ = std::make_unique<TcpClientTransport>(host_, port_);
    
    if (!transport_->connect()) {
        logger_.Error("discovery_client.cpp: Failed to connect to DiscoveryService at {}:{}", host_, port_);
        if (failure_cb_) failure_cb_("Could not connect to DiscoveryService.");
        return;
    }

    logger_.Info("discovery_client.cpp: Connected to DiscoveryService. Sending registration for component type '{}'.", discovery_protocol::component_type_to_string(component_type_));
    
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

    if (encoded_msg.empty()) {
        logger_.Error("discovery_client.cpp: Encoded message is empty!");
        if (failure_cb_) failure_cb_("Encoded message is empty.");
        return;
    }

    logger_.Info("discovery_client.cpp: Sending encoded registration message ({} bytes)", encoded_msg.size());
    
    std::vector<uint8_t> reg_data(encoded_msg.begin(), encoded_msg.end());
    transport_->async_send_data(std::move(reg_data));

    // Wait for the registration response
    if (transport_->data_available(5000)) { // 5 second timeout
        std::vector<uint8_t> buffer(4096);
        int bytes_received = transport_->receive_data(buffer.data(), buffer.size() - 1);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            std::string response_str(reinterpret_cast<char*>(buffer.data()));
            logger_.Info("discovery_client.cpp: Received raw response from DiscoveryService: '{}'", response_str);

            discovery_protocol::Message response_msg;
            if (discovery_protocol::decode_message(response_str, response_msg) == discovery_protocol::ProtocolStatus::OK) {
                logger_.Info("discovery_client.cpp: Successfully decoded message with type: {}", static_cast<int>(response_msg.type));
                if (response_msg.type == discovery_protocol::MessageType::REGISTRATION_RESPONSE) {
                    auto& resp_payload = std::get<discovery_protocol::RegistrationResponse>(response_msg.data);
                    if (resp_payload.responseCode == discovery_protocol::ResponseCode::SUCCESS) {
                        logger_.Info("discovery_client.cpp: Discovery successful. Bridge target at {}:{}", resp_payload.connectionTargetAddress, resp_payload.connectionTargetPort);
                        try {
                            int bridge_port = std::stoi(resp_payload.connectionTargetPort);
                            registered_ = true;
                            if (success_cb_) {
                                success_cb_(resp_payload.connectionTargetAddress, bridge_port);
                            }
                            // Optionally update interval if provided via config JSON (future enhancement)
                        } catch (const std::invalid_argument& e) {
                            logger_.Error("discovery_client.cpp: Invalid port number received from DiscoveryService: '{}'", resp_payload.connectionTargetPort);
                            if (failure_cb_) failure_cb_("Invalid port number from DiscoveryService.");
                            return;
                        }
                    } else {
                        logger_.Error("discovery_client.cpp: Registration denied by DiscoveryService: {}", resp_payload.humanReadableMessage);
                        if (failure_cb_) failure_cb_("Registration denied: " + resp_payload.humanReadableMessage);
                        return;
                    }
                } else {
                    logger_.Error("discovery_client.cpp: Unexpected message type after registration: {}", static_cast<int>(response_msg.type));
                    if (failure_cb_) failure_cb_("Unexpected message type from DiscoveryService.");
                    return;
                }
            } else {
                logger_.Error("discovery_client.cpp: Failed to decode response from DiscoveryService. Raw message: '{}'", response_str);
                if (failure_cb_) failure_cb_("Failed to decode response.");
                return;
            }
        } else {
            logger_.Error("discovery_client.cpp: No data received from DiscoveryService or connection lost.");
            if (failure_cb_) failure_cb_("No response from DiscoveryService.");
            return;
        }
    } else {
        logger_.Warn("discovery_client.cpp: Timed out waiting for response from DiscoveryService.");
        if (failure_cb_) failure_cb_("Timeout waiting for DiscoveryService response.");
        return;
    }

    // Keepalive loop: send ping if no data received within interval
    logger_.Info("discovery_client.cpp: Entering keepalive loop (interval {} ms)", keepalive_interval_ms_.count());
    auto last_ping_sent = std::chrono::steady_clock::now();

    while (running_ && registered_) {
        auto now = std::chrono::steady_clock::now();
        int wait_ms = static_cast<int>(keepalive_interval_ms_.count());
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
                        if (pong.nextIntervalMs > 0) {
                            keepalive_interval_ms_ = std::chrono::milliseconds(pong.nextIntervalMs);
                        }
                        last_ping_sent = now; // reset timer
                    } else {
                        logger_.Info("discovery_client.cpp: Ignoring non-keepalive message type {} during keepalive phase", static_cast<int>(incoming.type));
                    }
                } else {
                    logger_.Warn("discovery_client.cpp: Failed to decode incoming keepalive-phase message ({} bytes)", bytes);
                }
            }
        } else {
            // Timeout -> send keepalive ping
            discovery_protocol::KeepalivePing ping_payload;
            //component id should be groupID.IDingroup (there is no function to format this in discovery_protocol)
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