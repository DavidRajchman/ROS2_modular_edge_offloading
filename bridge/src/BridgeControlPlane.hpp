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

// Bridge configuration constants
const uint16_t OM_PORT = 8100;                              // OM server port (deprecated - now from Discovery)
const std::string BRIDGE_COMPONENT_ID = "15:10";            // Bridge component ID (group 15, id 10)
const std::string DISCOVERY_SERVICE_HOST = "192.168.50.114"; // Discovery Service address (only fixed IP in system)
const uint16_t DISCOVERY_SERVICE_PORT = 9090;               // Discovery Service port
const uint16_t MGWCP_SERVER_PORT = 7000;                    // Bridge's MGWCP server port for MGW connections

// BridgeControlPlane - Central coordinator for offloading system
// Responsibilities:
// 1. Register with Discovery Service to obtain OM address and global config
// 2. Connect to OM for offloading decision coordination
// 3. Accept MGWCP connections from VHC and MEC gateways
// 4. Forward offloading requests from VHC to OM
// 5. Process session approvals/denials from OM
// 6. Create data plane routing rules between VHC and MEC
// 7. Manage data plane transport handlers for message forwarding
class BridgeControlPlane {
public:
    BridgeControlPlane();
    ~BridgeControlPlane();
    
    // Main lifecycle methods
    bool start();  // Four-phase startup: Discovery→OM→MGWCP→Maintenance
    void stop();   // Graceful shutdown of all threads and connections
    bool is_running() const { return running_.load(); }
    
    // Component status queries
    bool is_discovery_connected() const { return discovery_connected_.load(); }
    bool is_om_connected() const { return om_connected_.load(); }
    size_t get_mgwcp_connection_count() const;  // Active VHC/MEC connections

private:
    // === Startup sequence phases (called in order by start()) ===
    bool register_with_discovery_service();  // Phase 1: Get OM address and global config
    bool connect_to_om();                    // Phase 2: Connect to OM using address from Discovery
    bool start_mgwcp_server();               // Phase 3: Start TCP server for VHC/MEC connections
    void start_periodic_tasks();             // Phase 4: Launch maintenance thread
    
    // === Discovery Service integration ===
    bool perform_discovery_registration();   // Send REGISTRATION_REQUEST, handle WAIT retry, parse response
    void discovery_keepalive_thread_func();  // Send periodic KEEPALIVE_PING to maintain registration
    
    // === OM communication (control plane coordination) ===
    void om_connection_thread_func();        // Receive thread for OM messages (SESSION_APPROVED/DENIED)
    void handle_om_message(const nlohmann::json& message);  // Dispatch OM message by code
    void send_om_message(const nlohmann::json& message);    // Send JSON message to OM with mutex protection
    
    // === MGWCP server management (VHC/MEC connections) ===
    void mgwcp_server_thread_func();         // Process MGWCP server events (new connections, disconnections)
    void handle_new_mgwcp_connection(uint32_t transport_client_id, const std::string& client_ip);  // Create MGWCPConnection
    void handle_mgwcp_disconnection(uint32_t transport_client_id);  // Cleanup session and connection state
    
    // === Message forwarding callbacks (used by MGWCPConnection) ===
    void forward_message_to_om(const std::string& mgwcp_component_id, const nlohmann::json& message);  // VHC→OM requests
    bool establish_mgwcp_data_plane_connection(const std::string& mgwcp_component_id,   // Create data plane transport handler
                                              const std::string& dp_host, int dp_port);
    
    // === Session and routing management ===
    void handle_session_approved(const nlohmann::json& om_response);  // Create routing rules, forward to VHC/MEC
    void handle_session_denied(const nlohmann::json& om_response);    // Forward denial to VHC
    void create_routing_rules_for_session(const std::string& request_id);  // Add VHC→MEC and MEC→VHC routes to routing table
    void remove_routing_rules_for_session(const std::string& request_id);  // Remove routes on session termination
    
    // === Data plane coordination ===
    bool create_transport_handler_for_mgwdp(const std::string& mgwcp_component_id,  // Create transport handler for VHC/MEC data plane
                                           const std::string& host, int port);
    bool create_transport_handler_for_mec(const std::string& mec_component_id,      // Create transport handler for MEC (if needed)
                                         const std::string& host, int port);
    
    // === Periodic maintenance (currently disabled for sessions) ===
    void maintenance_thread_func();    // Main maintenance loop
    void check_session_timeouts();     // Find expired sessions (currently disabled - sessions not deleted)
    void cleanup_expired_sessions();   // Additional cleanup logic placeholder
    
    // === Core components ===
    std::shared_ptr<GlobalConfig> global_config_;      // Task database from Discovery Service (input/output message types)
    std::shared_ptr<SessionManager> session_manager_;  // Active session tracking (currently disabled)
    std::shared_ptr<RoutingTable> routing_table_;      // Data plane routing: (source_id, msg_type) → destination queues
    
    // === Threading and lifecycle ===
    std::atomic<bool> running_;              // Bridge operational status
    std::atomic<bool> shutdown_requested_;   // Shutdown signal for all threads
    
    // === Discovery Service connection ===
    std::unique_ptr<gateway::TcpClientTransport> discovery_transport_;  // TCP connection to Discovery Service
    std::atomic<bool> discovery_connected_;                             // Discovery connection status
    std::thread discovery_keepalive_thread_;                            // Keepalive sender thread
    std::chrono::steady_clock::time_point last_discovery_keepalive_;    // Last keepalive timestamp
    
    // === OM connection (address obtained from Discovery Service) ===
    std::string om_host_;   // OM IP address from Discovery Service REGISTRATION_RESPONSE
    uint16_t om_port_;      // OM port from Discovery Service REGISTRATION_RESPONSE
    std::unique_ptr<gateway::TcpClientTransport> om_transport_;  // TCP connection to OM
    std::atomic<bool> om_connected_;                             // OM connection status
    std::thread om_connection_thread_;                           // OM message receiver thread
    std::mutex om_send_mutex_;                                   // Protects om_transport_ sends
    
    // === MGWCP server (accepts VHC/MEC connections) ===
    std::shared_ptr<gateway::TcpServerTransport> mgwcp_server_;  // TCP server on port 7000
    std::thread mgwcp_server_thread_;                            // Server event processing thread
    
    // === MGWCP connections management ===
    mutable std::mutex mgwcp_connections_mutex_;                                    // Protects connection maps
    std::unordered_map<uint32_t, std::unique_ptr<MGWCPConnection>> mgwcp_connections_;  // transport_id → MGWCPConnection
    std::unordered_map<std::string, uint32_t> component_id_to_transport_id_;        // "60:5" → transport_id lookup
    uint32_t next_mgwcp_connection_id_;                                             // Connection ID counter
    
    // === Data plane transport handlers (VHC/MEC data plane connections) ===
    std::mutex transport_handlers_mutex_;                                           // Protects transport_handlers_ map
    std::unordered_map<std::string, std::shared_ptr<TransportHandler>> transport_handlers_;  // component_id → TransportHandler
    
    // === Maintenance thread ===
    std::thread maintenance_thread_;  // Periodic cleanup and timeout checking
    
    // === Configuration constants ===
    static constexpr std::chrono::seconds DISCOVERY_KEEPALIVE_INTERVAL_{5};  // Discovery Service keepalive interval
    static constexpr std::chrono::seconds SESSION_TIMEOUT_{20};              // Session timeout (currently disabled)
    static constexpr std::chrono::seconds MAINTENANCE_INTERVAL_{10};         // Maintenance check interval
    static constexpr size_t MAX_MGWCP_CONNECTIONS = 50;                      // Maximum VHC/MEC connections
};

#endif // BRIDGE_CONTROL_PLANE_HPP