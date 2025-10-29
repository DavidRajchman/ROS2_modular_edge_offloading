#ifndef MGWCP_CONNECTION_HPP
#define MGWCP_CONNECTION_HPP

#include "SessionManager.hpp"
#include "logging/logger.h"
#include <nlohmann/json.hpp>
#include <transport/transport_base.hpp>
#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <chrono>
#include <functional>

// Forward declarations
class BridgeControlPlane;

// MGWCP connection state machine
// CONNECTING: Waiting for DP_INFO(103) message
// WAITING_FOR_DP: DP_INFO received but data plane connection failed
// OPERATIONAL: Data plane connected, can process OFFLOAD_REQUEST(100)
enum class MGWCPConnectionState {
    CONNECTING,
    WAITING_FOR_DP,
    OPERATIONAL
};

// Pending ACK tracking for reliable messaging
// Stores message, send time, and retry count for 5s timeout/3 retry logic
struct PendingAck {
    nlohmann::json message;           // Original message for retransmission
    std::chrono::steady_clock::time_point time_sent;  // Last send time
    int retry_count;                  // Number of retries attempted
};

class MGWCPConnection {
public:
    // Callback to forward VHC/MEC messages to OM (OFFLOAD_REQUEST, KEEPALIVE, TERMINATE)
    using MessageForwarder = std::function<void(const std::string&, const nlohmann::json&)>;
    // Callback to create TransportHandler for gateway's data plane
    using DataPlaneConnectCallback = std::function<bool(const std::string&, const std::string&, int)>;
    // Callback to register component_id→transport_id mapping in BridgeControlPlane
    using ComponentRegistrationCallback = std::function<void(const std::string&, uint32_t)>;
    
    MGWCPConnection(uint32_t connection_id,
                   std::shared_ptr<gateway::TcpServerTransport> server_transport,
                   uint32_t transport_client_id,
                   const std::string& client_ip,
                   std::shared_ptr<SessionManager> session_manager,
                   MessageForwarder message_forwarder,
                   DataPlaneConnectCallback dp_connect_callback,
                   ComponentRegistrationCallback component_registration_callback);
    
    ~MGWCPConnection();
    
    // Connection lifecycle
    void start();
    void stop();
    bool is_running() const { return running_.load(); }
    
    // Send messages to MGWCP (from OM via BridgeControlPlane)
    bool send_session_approved(const std::string& request_id, const nlohmann::json& payload);
    bool send_session_denied(const std::string& request_id, const std::string& reason);
    bool send_dp_connection_confirmed();
    
    // Connection info
    uint32_t get_connection_id() const { return connection_id_; }
    std::string get_component_id() const { return component_id_; }
    std::string get_client_ip() const { return client_ip_; }
    MGWCPConnectionState get_state() const { return state_; }
    
    // Data plane information
    std::string get_dp_host() const { return dp_host_; }
    int get_dp_port() const { return dp_port_; }

private:
    void connection_thread_func();
    void handle_received_message(const nlohmann::json& message);
    void handle_dp_info(const nlohmann::json& payload);
    void handle_offload_request(const nlohmann::json& payload);
    void handle_session_keepalive(const nlohmann::json& payload);
    void handle_session_terminate_request(const nlohmann::json& payload);
    void handle_ack(const nlohmann::json& payload);
    void send_reliable_message(nlohmann::json& message);
    void send_ack(uint64_t ack_sequence_number);
    void check_for_timeouts();
    bool validate_message_structure(const nlohmann::json& message);
    bool is_valid_component_id(const std::string& component_id);
    void set_state(MGWCPConnectionState new_state);
    
    // Bridge-assigned connection ID (unique per connection)
    uint32_t connection_id_;
    // MGWCP server transport (shared by all connections)
    std::shared_ptr<gateway::TcpServerTransport> server_transport_;
    // Transport layer client ID for this specific connection
    uint32_t transport_client_id_;
    // Auto-detected client IP (used if dp_host is 0.0.0.0)
    std::string client_ip_;
    // Gateway's component_id "group:id" extracted from first message
    std::string component_id_;
    
    // Current connection state (CONNECTING/WAITING_FOR_DP/OPERATIONAL)
    std::atomic<MGWCPConnectionState> state_;
    // Data plane host (from DP_INFO or auto-detected)
    std::string dp_host_;
    // Data plane port (from DP_INFO)
    int dp_port_;
    
    // Connection thread running flag
    std::atomic<bool> running_;
    // Message receiver thread
    std::thread connection_thread_;
    
    // Session tracking for keepalive updates
    std::shared_ptr<SessionManager> session_manager_;
    // Callback to forward messages to OM
    MessageForwarder message_forwarder_;
    // Callback to create TransportHandler
    DataPlaneConnectCallback dp_connect_callback_;
    // Callback to register component_id mapping
    ComponentRegistrationCallback component_registration_callback_;
    
    // Protects pending_acks_ map
    std::mutex pending_acks_mutex_;
    // seq_num → PendingAck (for retry logic)
    std::unordered_map<uint64_t, PendingAck> pending_acks_;
    // Incrementing sequence number for outgoing messages
    std::atomic<uint64_t> sequence_number_;
    
    // ACK timeout before retry
    static constexpr std::chrono::seconds ACK_TIMEOUT_{5};
    // Maximum retries before giving up
    static constexpr int MAX_RETRY_COUNT = 3;
    // Maximum JSON message size
    static constexpr size_t MAX_MESSAGE_SIZE = 8192;
};

#endif // MGWCP_CONNECTION_HPP