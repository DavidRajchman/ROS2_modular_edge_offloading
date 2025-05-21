#ifndef TRANSPORT_HANDLER_HPP
#define TRANSPORT_HANDLER_HPP

#include "common_types.hpp"       // For Message, RoutingKey, MPSCQueueType, ParsedHeaderInfo, parse_modular_gw_header
#include "RoutingTable.hpp"       // For RoutingTable
#include "ITransportHandlerObserver.hpp" // For the observer interface (even if not fully used yet)
#include <transport/transport_base.hpp> // From your transportlib
#include <transport/logging_utils.hpp>    // For LOG_INFO, LOG_ERROR etc.

#include <string>
#include <memory>   // For std::shared_ptr, std::weak_ptr, std::enable_shared_from_this
#include <thread>   // For std::thread
#include <atomic>   // For std::atomic_bool
#include <vector>   // For std::vector<unsigned char> buffer

// Forward declaration if ITransportHandlerObserver is complex or to reduce include dependencies in some cases,
// but since we need its type for weak_ptr, including the header is fine.
// class ITransportHandlerObserver;

class TransportHandler : public std::enable_shared_from_this<TransportHandler> {
public:
    TransportHandler(
        std::string gateway_id,
        std::string target_ip,
        int target_port,
        std::shared_ptr<MPSCQueueType> input_queue, // Messages from Bridge to this GW
        std::shared_ptr<RoutingTable> routing_table,
        std::weak_ptr<ITransportHandlerObserver> observer, // For future notifications to CP
        // Configuration parameters
        int connect_max_retries = 5,
        int connect_retry_delay_ms = 1000,
        size_t receive_buffer_size = 8192 // 8KB default buffer for incoming data
    );

    ~TransportHandler();

    // Starts the handler's internal thread to connect and process messages
    void start();

    // Signals the handler to stop, disconnect, and join its thread
    void stop();

    // Returns the Gateway ID this handler is responsible for
    std::string get_gateway_id() const;

    // Checks if the handler's internal thread believes it's connected
    bool is_connected() const;

private:
    // Core logic executed in the dedicated thread
    void run_internal();

    // Connection attempt logic, including handshake
    bool attempt_connection();
    bool perform_handshake(); // Placeholder for handshake logic

    // Message handling logic
    void handle_incoming_data(); // Reads from socket, parses, routes
    void handle_outgoing_messages(); // Reads from input_queue_, sends to socket

    // Helper to safely notify observer (will be used more when observer logic is added)
    void notify_observer_connected();
    void notify_observer_disconnected(const std::string& reason);
    void notify_observer_critical_error(const std::string& error_message);


    // --- Member Variables ---
    std::string gateway_id_;
    std::string target_ip_;
    int target_port_;

    std::shared_ptr<MPSCQueueType> input_queue_; // Consume from this queue to send to GW
    std::shared_ptr<RoutingTable> routing_table_; // Use this to route messages received from GW
    std::weak_ptr<ITransportHandlerObserver> cp_observer_weak_; // Stored for future use

    // Transport layer
    std::unique_ptr<gateway::TcpClientTransport> tcp_client_;
    std::vector<unsigned char> receive_buffer_; // Buffer for accumulating data from socket
    size_t receive_buffer_watermark_; // How much valid data is in receive_buffer_

    // Threading and state
    std::thread handler_thread_;
    std::atomic<bool> shutdown_requested_;
    std::atomic<bool> connected_status_; // Reflects TCP + handshake status

    // Configuration
    int connect_max_retries_;
    int connect_retry_delay_ms_;
    const size_t MAX_RECEIVE_BUFFER_SIZE; // Max size for receive_buffer_

    // Constants for handshake (example, to be defined)
    static constexpr char HANDSHAKE_MSG_BRIDGE_HELLO[] = "BRIDGE_HELLO_V1";
    static constexpr char HANDSHAKE_MSG_GW_ACK[] = "GW_ACK_V1";
};

#endif // TRANSPORT_HANDLER_HPP