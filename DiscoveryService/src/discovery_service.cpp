#include "discovery_service.hpp"
#include <iostream>
#include <sstream>
#include <transport/logging_utils.hpp>

DiscoveryService::DiscoveryService(uint16_t port)
    : port_(port), last_purge_time_(std::chrono::steady_clock::now()), shutting_down_(false) {
    // The constructor for TcpServerTransport is:
    // TcpServerTransport(int port, bool multi_client_mode = false, int max_clients = 0);
    // We enable multi-client mode to handle multiple components registering.
    transport_ = std::make_unique<gateway::TcpServerTransport>(port, true, 0); // 0 for max_clients means system limit
}

void DiscoveryService::start() {
    // Corrected function names: set_connect_callback, set_disconnect_callback, set_data_callback
    transport_->set_connect_callback([this](uint32_t id, const std::string&, int) { onClientConnected(id); });
    transport_->set_disconnect_callback([this](uint32_t id) { onClientDisconnected(id); });
    transport_->set_data_callback([this](uint32_t id) { 
        // This callback just signals data is available. We need to read it.
        std::vector<uint8_t> buffer(1024);
        int bytes_read = transport_->receive_from(id, buffer.data(), buffer.size());
        if (bytes_read > 0) {
            buffer.resize(bytes_read);
            onDataReceived(id, buffer);
        }
    });

    LOG_INFO("Discovery Service starting on port %u...", port_);
    if (!transport_->connect()) { // Use connect() to start the server
        LOG_ERROR("Failed to start TCP server on port %u.", port_);
        return;
    }
    
    // The main event loop now periodically checks for stale clients.
    while (!shutting_down_.load() && transport_->is_connected()) { // Use is_connected() to check server status
        // Process network events with a 1-second timeout.
        transport_->process_events(1000);

        // Check if it's time to purge stale clients.
        auto now = std::chrono::steady_clock::now();
        if (now - last_purge_time_ > KEEPALIVE_TIMEOUT / 2) {
            purgeStaleClients();
            last_purge_time_ = now;
        }
    }
}

void DiscoveryService::stop() {
    bool expected = false;
    // Atomically check and set the flag. Only proceed if we are the first to call stop().
    if (!shutting_down_.compare_exchange_strong(expected, true)) {
        return; // Already shutting down, do nothing.
    }

    if (transport_) {
        LOG_INFO("Stopping Discovery Service...");
        transport_->disconnect();
    }
}

void DiscoveryService::purgeStaleClients() {
    LOG_DEBUG("Running periodic check for stale clients...");
    const auto now = std::chrono::steady_clock::now();
    auto client_ids = registry_.get_all_client_ids();

    for (uint32_t client_id : client_ids) {
        auto component_opt = registry_.find_by_client_id(client_id);
        if (component_opt) {
            auto& component = *component_opt;
            auto time_since_last_seen = now - component.last_seen;
            if (time_since_last_seen > KEEPALIVE_TIMEOUT) {
                LOG_WARN("Client '%s' (%u.%u) timed out. Last seen %.2f seconds ago. Disconnecting.",
                         component.name.c_str(), component.group_id, component.id_in_group,
                         std::chrono::duration<double>(time_since_last_seen).count());
                
                transport_->disconnect_client(client_id);
            }
        }
    }
}

void DiscoveryService::onClientConnected(uint32_t client_id) {
    LOG_INFO("Client connected with transport ID: %u", client_id);
}


void DiscoveryService::onClientDisconnected(uint32_t client_id) {
    auto component = registry_.find_by_client_id(client_id);
    if (component) {
        LOG_INFO("Registered component '%s' (ID: %u.%u, Transport ID: %u) disconnected.",
                 component->name.c_str(), component->group_id, component->id_in_group, client_id);
        registry_.unregister_component(client_id);

        // Only call stop() once
        if (component->component_type == discovery_protocol::ComponentType::OFFLOAD_MANAGER
            && !shutting_down_.load()) {
            LOG_ERROR("The Offloading Manager has disconnected. This is a critical failure. Shutting down the system.");
            stop();
        }
    } else {
        LOG_INFO("Unregistered client disconnected with transport ID: %u", client_id);
    }
}

void DiscoveryService::onDataReceived(uint32_t client_id, const std::vector<uint8_t>& data) {
    std::string message_str(data.begin(), data.end());
    discovery_protocol::Message message;
    discovery_protocol::ProtocolStatus status = discovery_protocol::decode_message(message_str, message);

    if (status != discovery_protocol::ProtocolStatus::OK) {
        LOG_ERROR("Failed to decode message from client %u. Error: %s",
                  client_id, discovery_protocol::protocol_status_to_string(status));
        discovery_protocol::ErrorMessage err_msg;
        err_msg.errorCode = discovery_protocol::ErrorCode::PROTOCOL;
        err_msg.humanReadableMessage = "Malformed or invalid message received.";
        std::string response_str;
        discovery_protocol::encode_message(discovery_protocol::Message(err_msg), response_str);
        sendResponse(client_id, response_str);
        return;
    }

    switch (message.type) {
        case discovery_protocol::MessageType::REGISTRATION_REQUEST:
            handleRegistration(client_id, std::get<discovery_protocol::RegistrationRequest>(message.data));
            break;
        case discovery_protocol::MessageType::KEEPALIVE_PING:
            handleKeepalive(client_id, std::get<discovery_protocol::KeepalivePing>(message.data));
            break;
        default:
            LOG_WARN("Received unhandled message type from client %u", client_id);
            break;
    }
}

void DiscoveryService::handleRegistration(uint32_t client_id, const discovery_protocol::RegistrationRequest& req) {
    LOG_INFO("Processing REG request from client %u for component '%s'", client_id, req.componentName.c_str());

    discovery_protocol::RegistrationResponse resp;
    resp.assignedGroupId = req.groupId;
    resp.assignedIdInGroup = req.idInGroup;

    // Special handling for the Offloading Manager
    if (req.componentType == discovery_protocol::ComponentType::OFFLOAD_MANAGER) {
        global_configuration_ = req.humanReadableMessage;
        LOG_INFO("Offloading Manager registered. Global configuration has been set.");
    } else {
        // All other components must wait for the OM to provide the configuration
        if (!global_configuration_.has_value()) {
            LOG_WARN("Component '%s' trying to register before OM. Sending WAIT.", req.componentName.c_str());
            resp.responseCode = discovery_protocol::ResponseCode::WAIT;
            resp.humanReadableMessage = "Waiting for system configuration from Offloading Manager.";
            std::string response_str;
            discovery_protocol::encode_message(discovery_protocol::Message(resp), response_str);
            sendResponse(client_id, response_str);
            return;
        }
    }

    // Check for ID conflicts
    if (registry_.is_id_taken(req.groupId, req.idInGroup)) {
        LOG_ERROR("ID conflict for component '%s'. ID %u.%u is already taken.", req.componentName.c_str(), req.groupId, req.idInGroup);
        resp.responseCode = discovery_protocol::ResponseCode::ID_CONFLICT;
        resp.humanReadableMessage = "Component ID is already in use.";
        std::string response_str;
        discovery_protocol::encode_message(discovery_protocol::Message(resp), response_str);
        sendResponse(client_id, response_str);
        return;
    }

    // For Vehicles or MECs, check if a Bridge is available
    if (req.componentType == discovery_protocol::ComponentType::VEHICLE || req.componentType == discovery_protocol::ComponentType::MEC) {
        auto bridge_info_opt = registry_.find_available_bridge();
        if (!bridge_info_opt) {
            LOG_WARN("Component '%s' (VHC/MEC) trying to register, but no Bridge is available. Sending WAIT.", req.componentName.c_str());
            resp.responseCode = discovery_protocol::ResponseCode::WAIT;
            resp.humanReadableMessage = "No Bridge component is currently available.";
            std::string response_str;
            discovery_protocol::encode_message(discovery_protocol::Message(resp), response_str);
            sendResponse(client_id, response_str);
            return;
        } else {
            auto& bridge_info = *bridge_info_opt;
            // Populate Bridge connection info in the response
            resp.connectionTargetType = discovery_protocol::component_type_to_string(bridge_info.component_type);
            resp.connectionTargetAddress = bridge_info.listen_address;
            resp.connectionTargetPort = std::to_string(bridge_info.listen_port);
            resp.connectionTargetId = (bridge_info.group_id << 8) | bridge_info.id_in_group;
        }
    }

    // All checks passed, register the component
    ComponentInfo info;
    info.client_id = client_id;
    info.component_type = req.componentType;
    info.group_id = req.groupId;
    info.id_in_group = req.idInGroup;
    info.name = req.componentName;
    info.listen_address = req.listenAddress;
    try {
        info.listen_port = static_cast<uint16_t>(std::stoi(req.listenPort));
    } catch (const std::exception& e) {
        LOG_ERROR("Invalid listen port '%s' for component '%s'.", req.listenPort.c_str(), req.componentName.c_str());
        resp.responseCode = discovery_protocol::ResponseCode::INVALID_REQUEST;
        resp.humanReadableMessage = "Invalid listen port format.";
        std::string response_str;
        discovery_protocol::encode_message(discovery_protocol::Message(resp), response_str);
        sendResponse(client_id, response_str);
        return;
    }
    info.last_seen = std::chrono::steady_clock::now();

    if (registry_.register_component(info)) {
        LOG_INFO("Successfully registered component '%s' with ID %u.%u", info.name.c_str(), info.group_id, info.id_in_group);
        resp.responseCode = discovery_protocol::ResponseCode::SUCCESS;
        resp.configJson = global_configuration_.value_or("");
        resp.humanReadableMessage = "Registration successful.";
    } else {
        // This case should be rare due to the earlier is_id_taken check, but is a safeguard.
        LOG_ERROR("Failed to register component '%s' due to an internal registry error.", info.name.c_str());
        resp.responseCode = discovery_protocol::ResponseCode::GENERAL_ERROR;
        resp.humanReadableMessage = "An internal server error occurred during registration.";
    }

    std::string response_str;
    discovery_protocol::encode_message(discovery_protocol::Message(resp), response_str);
    sendResponse(client_id, response_str);
}

void DiscoveryService::handleKeepalive(uint32_t client_id, const discovery_protocol::KeepalivePing& ping) {
    // The ping contains componentId as "group.id"
    std::stringstream ss(ping.componentId);
    int group_id, id_in_group;
    char dot;
    ss >> group_id >> dot >> id_in_group;

    if (ss.fail() || dot != '.') {
        LOG_ERROR("Received malformed Keepalive ID '%s' from client %u", ping.componentId.c_str(), client_id);
        return;
    }

    if (registry_.update_keepalive(group_id, id_in_group)) {
        LOG_DEBUG("Keepalive received from component %u.%u", group_id, id_in_group);
        
        discovery_protocol::KeepaliveResponse pong;
        pong.response = "OK";
        pong.nextIntervalMs = 5000;
        pong.humanReadableMessage = "Keepalive acknowledged.";
        
        std::string response_str;
        discovery_protocol::encode_message(discovery_protocol::Message(pong), response_str);
        sendResponse(client_id, response_str);
    } else {
        LOG_WARN("Received keepalive for unknown or unregistered component ID %u.%u from client %u", group_id, id_in_group, client_id);
    }
}

void DiscoveryService::sendResponse(uint32_t client_id, const std::string& message) {
    std::vector<uint8_t> data(message.begin(), message.end());
    if (!transport_->send_to(client_id, data.data(), data.size())) {
        LOG_ERROR("Failed to send response to client %u", client_id);
    }
}