#ifndef BRIDGE_CONTROL_PLANE_HPP
#define BRIDGE_CONTROL_PLANE_HPP

#include "GlobalConfig.hpp"
#include "SessionManager.hpp"
#include "MGWCPConnection.hpp"
#include "TransportHandler.hpp"
#include "RoutingTable.hpp"
#include "common_types.hpp"
#include "logging/logger.h"

#include <discovery_protocol/protocol.hpp>
#include <transport/transport_base.hpp>
#include <nlohmann/json.hpp>

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <chrono>

// Constants
const uint16_t OM_PORT = 8100;
const std::string BRIDGE_COMPONENT_ID = "15:10";
const std::string DISCOVERY_SERVICE_HOST = "192.168.65.5";
const uint16_t DISCOVERY_SERVICE_PORT = 9090;
const uint16_t MGWCP_SERVER_PORT = 7000;

class BridgeControlPlane {
public:
    BridgeControlPlane();
    ~BridgeControlPlane();
    
    // Main lifecycle
    bool start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Get component status
    bool is_discovery_connected() const { return discovery_connected_.load(); }
    bool is_om_connected() const { return om_connected_.load(); }
    size_t get_mgwcp_connection_count() const;

private:
    // Startup sequence phases
    bool register_with_discovery_service();
    bool connect_to_om();
    bool start_mgwcp_server();
    void start_periodic_tasks();
    
    // Discovery Service integration
    bool perform_discovery_registration();
    void discovery_keepalive_thread_func();
    
    // OM communication
    void om_connection_thread_func();
    void handle_om_message(const nlohmann::json& message);
    void send_om_message(const nlohmann::json& message);
    
    // MGWCP server management
    void mgwcp_server_thread_func();
    void handle_new_mgwcp_connection(uint32_t transport_client_id, const std::string& client_ip);
    void handle_mgwcp_disconnection(uint32_t transport_client_id);
    
    // Message forwarding (callbacks for MGWCPConnection)
    void forward_message_to_om(const std::string& mgwcp_component_id, const nlohmann::json& message);
    bool establish_mgwcp_data_plane_connection(const std::string& mgwcp_component_id, 
                                              const std::string& dp_host, int dp_port);
    
    // Session and routing management
    void handle_session_approved(const nlohmann::json& om_response);
    void handle_session_denied(const nlohmann::json& om_response);
    void create_routing_rules_for_session(const std::string& request_id);
    void remove_routing_rules_for_session(const std::string& request_id);
    
    // Data plane coordination
    bool create_transport_handler_for_mgwdp(const std::string& mgwcp_component_id, 
                                           const std::string& host, int port);
    bool create_transport_handler_for_mec(const std::string& mec_component_id, 
                                         const std::string& host, int port);
    
    // Periodic maintenance
    void maintenance_thread_func();
    void check_session_timeouts();
    void cleanup_expired_sessions();
    
    // Core components
    std::shared_ptr<GlobalConfig> global_config_;
    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<RoutingTable> routing_table_;
    
    // Threading and lifecycle
    std::atomic<bool> running_;
    std::atomic<bool> shutdown_requested_;
    
    // Discovery Service connection
    std::unique_ptr<gateway::TcpClientTransport> discovery_transport_;
    std::atomic<bool> discovery_connected_;
    std::thread discovery_keepalive_thread_;
    std::chrono::steady_clock::time_point last_discovery_keepalive_;
    
    // OM connection (now dynamic from Discovery Service)
    std::string om_host_;  // Set from Discovery Service response
    uint16_t om_port_;     // Set from Discovery Service response
    std::unique_ptr<gateway::TcpClientTransport> om_transport_;
    std::atomic<bool> om_connected_;
    std::thread om_connection_thread_;
    std::mutex om_send_mutex_;
    
    // MGWCP server
    std::shared_ptr<gateway::TcpServerTransport> mgwcp_server_;
    std::thread mgwcp_server_thread_;
    
    // MGWCP connections management
    mutable std::mutex mgwcp_connections_mutex_;
    std::unordered_map<uint32_t, std::unique_ptr<MGWCPConnection>> mgwcp_connections_;
    std::unordered_map<std::string, uint32_t> component_id_to_transport_id_;
    uint32_t next_mgwcp_connection_id_;
    
    // Data plane transport handlers
    std::mutex transport_handlers_mutex_;
    std::unordered_map<std::string, std::shared_ptr<TransportHandler>> transport_handlers_;
    
    // Maintenance thread
    std::thread maintenance_thread_;
    
    // Configuration
    static constexpr std::chrono::seconds DISCOVERY_KEEPALIVE_INTERVAL_{5};
    static constexpr std::chrono::seconds SESSION_TIMEOUT_{20};
    static constexpr std::chrono::seconds MAINTENANCE_INTERVAL_{10};
    static constexpr size_t MAX_MGWCP_CONNECTIONS = 50;
};

#endif // BRIDGE_CONTROL_PLANE_HPP