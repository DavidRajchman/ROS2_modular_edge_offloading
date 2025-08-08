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

// Connection states based on MGWCP state machine
enum class MGWCPConnectionState {
    CONNECTING,           // Initial TCP connection established
    WAITING_FOR_DP,      // Received DP_INFO, waiting for data plane connection
    OPERATIONAL          // Data plane connected, can handle offloading requests
};

// Pending ACK structure for reliability mechanism
struct PendingAck {
    nlohmann::json message;
    std::chrono::steady_clock::time_point time_sent;
    int retry_count;
};

class MGWCPConnection {
public:
    // Callback function types for communicating with BridgeControlPlane
    using MessageForwarder = std::function<void(const std::string&, const nlohmann::json&)>;
    using DataPlaneConnectCallback = std::function<bool(const std::string&, const std::string&, int)>;
    // New callback invoked when component_id is first learned so Bridge can register mapping
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
    // Main connection thread
    void connection_thread_func();
    
    // Message handling
    void handle_received_message(const nlohmann::json& message);
    void handle_dp_info(const nlohmann::json& payload);
    void handle_offload_request(const nlohmann::json& payload);
    void handle_session_keepalive(const nlohmann::json& payload);
    void handle_session_terminate_request(const nlohmann::json& payload);
    void handle_ack(const nlohmann::json& payload);
    
    // ACK reliability mechanism
    void send_reliable_message(nlohmann::json& message);
    void send_ack(uint64_t ack_sequence_number);
    void check_for_timeouts();
    
    // Message validation
    bool validate_message_structure(const nlohmann::json& message);
    bool is_valid_component_id(const std::string& component_id);
    
    // State management
    void set_state(MGWCPConnectionState new_state);
    
    // Connection details
    uint32_t connection_id_;
    std::shared_ptr<gateway::TcpServerTransport> server_transport_;
    uint32_t transport_client_id_;
    std::string client_ip_;
    std::string component_id_;  // Set from first valid message
    
    // State and data plane info
    std::atomic<MGWCPConnectionState> state_;
    std::string dp_host_;
    int dp_port_;
    
    // Threading
    std::atomic<bool> running_;
    std::thread connection_thread_;
    
    // Dependencies
    std::shared_ptr<SessionManager> session_manager_;
    MessageForwarder message_forwarder_;
    DataPlaneConnectCallback dp_connect_callback_;
    ComponentRegistrationCallback component_registration_callback_; // new
    
    // ACK reliability mechanism
    std::mutex pending_acks_mutex_;
    std::unordered_map<uint64_t, PendingAck> pending_acks_;
    std::atomic<uint64_t> sequence_number_;
    
    // Configuration
    static constexpr std::chrono::seconds ACK_TIMEOUT_{5};
    static constexpr int MAX_RETRY_COUNT = 3;
    static constexpr size_t MAX_MESSAGE_SIZE = 8192;
};

#endif // MGWCP_CONNECTION_HPP