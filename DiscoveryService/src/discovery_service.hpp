#pragma once

#include <memory>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>
#include <chrono>
#include <atomic>
#include <map>


#include <transport/transport_base.hpp>
#include <discovery_protocol/protocol.hpp>
#include "component_registry.hpp"

class DiscoveryService {
public:
    DiscoveryService(uint16_t port);
    void start();

private:
    void stop();

    // Callbacks for the transport layer
    void purgeStaleClients();

    void onClientConnected(uint32_t client_id, const std::string& ip_address);
    void onClientDisconnected(uint32_t client_id);
    void onDataReceived(uint32_t client_id, const std::vector<uint8_t>& data);

    // Handlers for specific message types from the protocol library
    void handleRegistration(uint32_t client_id, const discovery_protocol::RegistrationRequest& req);
    void handleKeepalive(uint32_t client_id, const discovery_protocol::KeepalivePing& ping);

    // Helper to send responses
    void sendResponse(uint32_t client_id, const std::string& message);
    // Define a timeout for keepalives
    static constexpr std::chrono::seconds KEEPALIVE_TIMEOUT{15};

    uint16_t port_;
    std::unique_ptr<gateway::TcpServerTransport> transport_;
    ComponentRegistry registry_;

    // The global configuration string provided by the Offloading Manager.
    std::optional<std::string> global_configuration_;

    // Map to store the auto-detected IP address for each client.
    std::map<uint32_t, std::string> client_ip_addresses_;

    // Timestamp for the last time the stale client check was performed.
    std::chrono::steady_clock::time_point last_purge_time_;

    // Flag to ensure shutdown logic is only run once.
    std::atomic<bool> shutting_down_{false};
};