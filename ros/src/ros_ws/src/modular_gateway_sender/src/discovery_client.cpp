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

    // Add validation to ensure we're not sending empty data
    if (encoded_msg.empty()) {
        logger_.Error("discovery_client.cpp: Encoded message is empty!");
        if (failure_cb_) failure_cb_("Encoded message is empty.");
        return;
    }

    logger_.Info("discovery_client.cpp: Sending encoded message ({} bytes): '{}'", encoded_msg.size(), encoded_msg);
    
    std::vector<uint8_t> reg_data(encoded_msg.begin(), encoded_msg.end());
    transport_->async_send_data(std::move(reg_data));

    // Wait for the response
    if (transport_->data_available(5000)) { // 5 second timeout
        std::vector<uint8_t> buffer(2048);
        int bytes_received = transport_->receive_data(buffer.data(), buffer.size() - 1);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate
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
                            if (success_cb_) {
                                success_cb_(resp_payload.connectionTargetAddress, bridge_port);
                            }
                        } catch (const std::invalid_argument& e) {
                            logger_.Error("discovery_client.cpp: Invalid port number received from DiscoveryService: '{}'", resp_payload.connectionTargetPort);
                            if (failure_cb_) failure_cb_("Invalid port number from DiscoveryService.");
                        }
                    } else {
                        logger_.Error("discovery_client.cpp: Registration denied by DiscoveryService: {}", resp_payload.humanReadableMessage);
                        if (failure_cb_) failure_cb_("Registration denied: " + resp_payload.humanReadableMessage);
                    }
                } else {
                    logger_.Error("discovery_client.cpp: Received unexpected message type from DiscoveryService. Expected: {}, Got: {}", 
                                static_cast<int>(discovery_protocol::MessageType::REGISTRATION_RESPONSE), 
                                static_cast<int>(response_msg.type));
                    if (failure_cb_) failure_cb_("Unexpected message type from DiscoveryService.");
                }
            } else {
                logger_.Error("discovery_client.cpp: Failed to decode response from DiscoveryService. Raw message: '{}'", response_str);
                if (failure_cb_) failure_cb_("Failed to decode response.");
            }
        } else {
            logger_.Error("discovery_client.cpp: No data received from DiscoveryService or connection lost.");
            if (failure_cb_) failure_cb_("No response from DiscoveryService.");
        }
    } else {
        logger_.Warn("discovery_client.cpp: Timed out waiting for response from DiscoveryService.");
        if (failure_cb_) failure_cb_("Timeout waiting for DiscoveryService response.");
    }
}

} // namespace gateway