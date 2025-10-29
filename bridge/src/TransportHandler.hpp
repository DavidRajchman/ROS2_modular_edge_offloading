#ifndef TRANSPORT_HANDLER_HPP
#define TRANSPORT_HANDLER_HPP

#include "common_types.hpp"       // For Message, RoutingKey, MPSCQueueType, ParsedHeaderInfo, parse_modular_gw_header
#include "RoutingTable.hpp"       // For RoutingTable
#include "ITransportHandlerObserver.hpp" // For the observer interface (even if not fully used yet)
#include <transport/transport_base.hpp> // From your transportlib
#include "logging/logger.h"    // For CppLogging

#include <string>
#include <memory>   // For std::shared_ptr, std::weak_ptr, std::enable_shared_from_this
#include <thread>   // For std::thread
#include <atomic>   // For std::atomic_bool
#include <vector>   // For std::vector<unsigned char> buffer
#include <chrono>

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
        size_t receive_buffer_size = 8192, // 8KB default buffer for incoming data
        int minimum_sleep_time_us = 200, //time the running loop will sleep for in us if no messages are received or sent
        int max_connect_cycles = 2 // Number of full attempt_connection() cycles before permanent failure
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

    // Expose the input queue so CP can add routing entries pointing to this handler
    std::shared_ptr<MPSCQueueType> get_input_queue() const;

private:
    void run_internal();
    bool attempt_connection();
    bool perform_handshake();
    void handle_incoming_data();
    bool handle_outgoing_messages();
    void notify_observer_connected();
    void notify_observer_disconnected(const std::string& reason);
    void notify_observer_critical_error(const std::string& error_message);

    // Gateway component_id this handler connects to (e.g., "60:5" or "70:1")
    std::string gateway_id_;
    // Target gateway IP address
    std::string target_ip_;
    // Target gateway data plane port
    int target_port_;

    // MPSC queue: Bridge→Gateway (outgoing binary messages)
    std::shared_ptr<MPSCQueueType> input_queue_;
    // Routing table for incoming messages: routes (source_id, msg_type) to destination queues
    std::shared_ptr<RoutingTable> routing_table_;
    // Observer for connection events (future use)
    std::weak_ptr<ITransportHandlerObserver> cp_observer_weak_;

    // TCP client for data plane connection
    std::unique_ptr<gateway::TcpClientTransport> tcp_client_;
    // Accumulator for partial binary frames from socket
    std::vector<unsigned char> receive_buffer_;
    // Number of valid bytes in receive_buffer_
    size_t receive_buffer_watermark_;

    // Handler thread for send/receive loop
    std::thread handler_thread_;
    // Shutdown signal flag
    std::atomic<bool> shutdown_requested_;
    // Connected status (TCP + handshake complete)
    std::atomic<bool> connected_status_;

    // Max retries per connection attempt
    int connect_max_retries_;
    // Delay between connection retries (milliseconds)
    int connect_retry_delay_ms_;
    // Maximum receive buffer size (default 8KB)
    const size_t MAX_RECEIVE_BUFFER_SIZE;
    // Sleep time when no messages (microseconds, default 200us)
    const int minimum_sleep_time_us_;
    // Maximum full connection cycles before permanent failure
    int max_connect_cycles_;
    // Number of failed connection cycles so far
    int failed_connect_cycles_;

    // Logger instance
    CppLogging::Logger logger_;

    // Handshake protocol messages
    static constexpr char HANDSHAKE_MSG_BRIDGE_HELLO[] = "BRIDGE_HELLO_V1";
    static constexpr char HANDSHAKE_MSG_GW_ACK[] = "GW_ACK_V1";
};

#endif // TRANSPORT_HANDLER_HPP