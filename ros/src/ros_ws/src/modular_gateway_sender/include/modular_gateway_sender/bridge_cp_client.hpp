#ifndef BRIDGE_CP_CLIENT_HPP
#define BRIDGE_CP_CLIENT_HPP

#include "logging/logger.h"
#include "modular_gateway_sender/transport_base.hpp"
#include <nlohmann/json.hpp>

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <map>
#include <chrono>

namespace gateway {

// Forward declare to avoid including the full transport header
class TransportBase;

// Structure to hold information about a message waiting for an ACK
struct PendingAck {
    nlohmann::json message;
    std::chrono::steady_clock::time_point time_sent;
};

class BridgeCpClient {
public:
    using json = nlohmann::json;

    // Callbacks to notify the GatewayController
    using DpConfirmedCallback = std::function<void()>;
    using SessionApprovedCallback = std::function<void(const nlohmann::json& payload)>;
    using SessionDeniedCallback = std::function<void(const std::string& request_id, const std::string& reason)>;

    BridgeCpClient();
    ~BridgeCpClient();

    // Connects to the Bridge's control plane endpoint and starts the client thread.
    bool start(
        const std::string& host, 
        int port,
        DpConfirmedCallback dp_confirmed_cb,
        SessionApprovedCallback session_approved_cb,
        SessionDeniedCallback session_denied_cb
    );

    // Stops the client thread and disconnects from the Bridge.
    void stop();

    // Public methods to send messages to the Bridge
    void send_dp_info(const std::string& component_id, const std::string& dp_host, int dp_port);
    void send_offload_request(const std::string& component_id, const std::string& request_id, const std::string& task_id, const std::string& task_name);
    void send_session_keepalive(const std::string& component_id, const std::string& request_id);
    void send_session_terminate_request(const std::string& component_id, const std::string& request_id);

private:
    // Main client thread for receiving messages and handling timeouts
    void client_thread_func();
    void handle_received_message(const json& msg);
    void check_for_timeouts();

    // Core message sending logic
    void send_reliable_message(json& msg);
    void send_ack(int ack_sequence_number, const std::string& component_id);

    std::unique_ptr<TransportBase> transport_;
    CppLogging::Logger logger_;

    std::thread client_thread_;
    std::atomic<bool> running_{false};

    // Reliability mechanism members
    std::atomic<uint64_t> sequence_number_{1};
    std::map<uint64_t, PendingAck> pending_acks_;
    std::mutex pending_acks_mutex_;
    const std::chrono::seconds ack_timeout_{5};

    // Callbacks
    DpConfirmedCallback on_dp_confirmed_;
    SessionApprovedCallback on_session_approved_;
    SessionDeniedCallback on_session_denied_;
};

} // namespace gateway

#endif // BRIDGE_CP_CLIENT_HPP