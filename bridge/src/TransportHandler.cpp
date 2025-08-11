#include "TransportHandler.hpp"
#include "common_types.hpp" // For parse_modular_gw_header, Message, etc.
#include <chrono> // For std::chrono::milliseconds
#include <vector> // For std::vector used in receive buffer operations
#include "logging/logger.h" // For CppLogging

// Define handshake constants if not already in header or a common place
// For this example, assuming they are in the header as static constexpr char[]
// const char* TransportHandler::HANDSHAKE_MSG_BRIDGE_HELLO = "BRIDGE_HELLO_V1";
// const char* TransportHandler::HANDSHAKE_MSG_GW_ACK = "GW_ACK_V1";


TransportHandler::TransportHandler(
    std::string gateway_id,
    std::string target_ip,
    int target_port,
    std::shared_ptr<MPSCQueueType> input_queue,
    std::shared_ptr<RoutingTable> routing_table,
    std::weak_ptr<ITransportHandlerObserver> observer,
    int connect_max_retries,
    int connect_retry_delay_ms,
    size_t receive_buffer_size,
    int minimum_sleep_time_us,
    int max_connect_cycles
) : gateway_id_(std::move(gateway_id)),
    target_ip_(std::move(target_ip)),
    target_port_(target_port),
    input_queue_(std::move(input_queue)),
    routing_table_(std::move(routing_table)),
    cp_observer_weak_(std::move(observer)),
    receive_buffer_watermark_(0),
    shutdown_requested_(false),
    connected_status_(false),
    connect_max_retries_(connect_max_retries),
    connect_retry_delay_ms_(connect_retry_delay_ms),
    MAX_RECEIVE_BUFFER_SIZE(receive_buffer_size), // Initialize const member
    minimum_sleep_time_us_(minimum_sleep_time_us),
    max_connect_cycles_(max_connect_cycles),
    failed_connect_cycles_(0),
    logger_("bridge")
{
    if (!input_queue_) {
        throw std::invalid_argument("TransportHandler: Input queue cannot be null.");
    }
    if (!routing_table_) {
        throw std::invalid_argument("TransportHandler: Routing table cannot be null.");
    }
    if (gateway_id_.empty()) {
        throw std::invalid_argument("TransportHandler: Gateway ID cannot be empty.");
    }
    if (target_ip_.empty()) {
        throw std::invalid_argument("TransportHandler: Target IP cannot be empty.");
    }
    if (target_port_ <= 0 || target_port_ > 65535) {
        throw std::invalid_argument("TransportHandler: Invalid target port.");
    }
    if (MAX_RECEIVE_BUFFER_SIZE == 0) {
        throw std::invalid_argument("TransportHandler: Receive buffer size cannot be zero.");
    }
    receive_buffer_.resize(MAX_RECEIVE_BUFFER_SIZE); // Pre-allocate receive buffer
    tcp_client_ = std::make_unique<gateway::TcpClientTransport>(target_ip_, target_port_, connect_max_retries_);
    logger_.Info("TransportHandler [{}]: Initialized for {}:{}.", gateway_id_, target_ip_, target_port_);
}

TransportHandler::~TransportHandler() {
    logger_.Info("TransportHandler [{}]: Destructor called. Ensuring shutdown.", gateway_id_);
    if (!shutdown_requested_.load()) {
        stop(); // Ensure stop is called if not already
    }
    // Thread should have been joined in stop()
}

void TransportHandler::start() {
    if (handler_thread_.joinable()) {
        logger_.Warn("TransportHandler [{}]: Start called but thread is already running.", gateway_id_);
        return;
    }
    shutdown_requested_.store(false);
    connected_status_.store(false);
    handler_thread_ = std::thread(&TransportHandler::run_internal, this);
    logger_.Info("TransportHandler [{}]: Started and spawned processing thread.", gateway_id_);
}

void TransportHandler::stop() {
    logger_.Info("TransportHandler [{}]: Stop requested.", gateway_id_);
    shutdown_requested_.store(true);

    // Optional: If there's a blocking call in run_internal (like a blocking receive or queue read),
    // you might need to interrupt it. For TCP, closing the socket from this thread
    // will typically cause blocking socket calls in the handler_thread_ to return with an error.
    if (tcp_client_ && tcp_client_->is_connected()) {
        logger_.Info("TransportHandler [{}]: Disconnecting client to unblock thread.", gateway_id_);
        tcp_client_->disconnect(); // This can help unblock socket operations in run_internal
    }
    
    // Optional: If input_queue_ supports a way to signal consumers (e.g. special shutdown message), use it.
    // For moodycamel::ConcurrentQueue, it doesn't have a built-in blocking dequeue with timeout that
    // can be easily interrupted other than by the shutdown_requested_ flag check.

    if (handler_thread_.joinable()) {
        logger_.Info("TransportHandler [{}]: Waiting for processing thread to join.", gateway_id_);
        try {
            handler_thread_.join();
            logger_.Info("TransportHandler [{}]: Processing thread joined.", gateway_id_);
        } catch (const std::system_error& e) {
            logger_.Error("TransportHandler [{}]: System error while joining thread: {}", gateway_id_, e.what());
        }
    } else {
        logger_.Info("TransportHandler [{}]: Processing thread was not joinable (already joined or not started).", gateway_id_);
    }
    connected_status_.store(false); // Ensure status is updated
}

std::string TransportHandler::get_gateway_id() const {
    return gateway_id_;
}

bool TransportHandler::is_connected() const {
    return connected_status_.load();
}

std::shared_ptr<MPSCQueueType> TransportHandler::get_input_queue() const {
    return input_queue_;
}

// --- Private Methods ---

void TransportHandler::run_internal() {
    logger_.Info("TransportHandler [{}]: Thread started execution.", gateway_id_);
    receive_buffer_watermark_ = 0; // Reset watermark
    bool work_done_this_iteration = false;

    while (!shutdown_requested_.load()) {
        if (!connected_status_.load()) {
            logger_.Info("TransportHandler [{}]: Not connected. Attempting connection...", gateway_id_);
            if (attempt_connection()) {
                logger_.Info("TransportHandler [{}]: Successfully connected to {}:{}.", gateway_id_, target_ip_, target_port_);
                connected_status_.store(true);
                failed_connect_cycles_ = 0; // Reset cycle failures on success
                notify_observer_connected(); // Placeholder for actual notification
            } else {
                failed_connect_cycles_++;
                logger_.Warn("TransportHandler [{}]: Connection attempt failed (cycle {} of {}). Will retry after delay.",
                             gateway_id_, failed_connect_cycles_, max_connect_cycles_);
                if (failed_connect_cycles_ >= max_connect_cycles_) {
                    logger_.Error("TransportHandler [{}]: Permanent connection failure after {} cycles. Giving up and marking handler inactive.",
                                   gateway_id_, failed_connect_cycles_);
                    notify_observer_disconnected("Permanent connection failure");
                    break; // Exit main loop -> cleanup
                }
                for (int i = 0; i < connect_retry_delay_ms_ / 100 && !shutdown_requested_.load(); ++i) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                continue; // Loop back to attempt another cycle
            }
        }

        // If connected, proceed to handle I/O
        if (connected_status_.load() && !shutdown_requested_.load()) {
            work_done_this_iteration = false;
            // Check for incoming data from the socket
            // The TcpClientTransport::data_available() can be used with a timeout.
            // A small timeout allows the loop to remain responsive to shutdown_requested_
            // and to check the outgoing queue.
            if (tcp_client_ && tcp_client_->data_available(0)) { // Check for data without a timeout
                handle_incoming_data();
                work_done_this_iteration = true; 
            } else if (tcp_client_ && !tcp_client_->is_connected()){
                logger_.Warn("TransportHandler [{}]: TCP client reported disconnected during data_available check.", gateway_id_);
                connected_status_.store(false);
                notify_observer_disconnected("TCP client disconnected");
                if(tcp_client_) tcp_client_->disconnect(); // Ensure it's fully closed
                continue; // Re-enter connection loop
            }

            // Check for outgoing messages from the input queue if work done, then log it
            work_done_this_iteration = work_done_this_iteration || handle_outgoing_messages();
            
            //if no work done, then sleep for certain amount of time in us
            if (!work_done_this_iteration){
                std::this_thread::sleep_for(std::chrono::microseconds(minimum_sleep_time_us_));
            }

        }
        
    }

    // Shutdown sequence
    logger_.Info("TransportHandler [{}]: Shutdown requested. Cleaning up...", gateway_id_);
    if (tcp_client_ && tcp_client_->is_connected()) {
        tcp_client_->disconnect();
    }
    connected_status_.store(false);
    logger_.Info("TransportHandler [{}]: Thread finished execution.", gateway_id_);
}

bool TransportHandler::attempt_connection() {
    if (!tcp_client_) {
        logger_.Error("TransportHandler [{}]: TCP client is null, cannot attempt connection.", gateway_id_);
        return false;
    }

    int current_retry = 0;
    while (current_retry < connect_max_retries_ && !shutdown_requested_.load()) {
        logger_.Info("TransportHandler [{}]: Attempting to connect ({}/{})...", gateway_id_, current_retry + 1, connect_max_retries_);
        if (tcp_client_->connect()) { // TcpClientTransport::connect() handles its own internal retries if configured
            logger_.Info("TransportHandler [{}]: TCP connection established. Performing handshake...", gateway_id_);
            // handshake is not implemented yet, it will always succeed for now
            if (perform_handshake()) {
                logger_.Info("TransportHandler [{}]: Handshake successful.", gateway_id_);
                return true; // Successfully connected and handshake complete
            } else {
                logger_.Warn("TransportHandler [{}]: Handshake failed. Disconnecting.", gateway_id_);
                tcp_client_->disconnect(); // Disconnect if handshake fails
                // No immediate retry for handshake failure in this loop, could be added
                return false; // Handshake failure means connection attempt failed for now
            }
        }
        current_retry++;
        if (current_retry < connect_max_retries_ && !shutdown_requested_.load()) {
            logger_.Info("TransportHandler [{}]: TCP connect failed. Retrying in {} ms.", gateway_id_, connect_retry_delay_ms_);
            for (int i = 0; i < connect_retry_delay_ms_ / 100 && !shutdown_requested_.load(); ++i) {
                 std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    logger_.Error("TransportHandler [{}]: Failed to connect after {} retries.", gateway_id_, connect_max_retries_);
    return false;
}

bool TransportHandler::perform_handshake() {
    // No explicit application-level handshake for now.
    // This function is called after TCP connection is established.
    // If we reach here, we consider the "handshake" (or lack thereof) successful.
    logger_.Info("TransportHandler [{}]: Skipping explicit handshake protocol.", gateway_id_);
    return true; // Always succeed as there's no handshake to fail
}

void TransportHandler::handle_incoming_data() {
    if (!tcp_client_ || !tcp_client_->is_connected()) {
        logger_.Warn("TransportHandler [{}]: Attempted to handle incoming data but not connected.", gateway_id_);
        connected_status_.store(false); // Ensure status is correct
        notify_observer_disconnected("Not connected during incoming data handling");
        return;
    }

    // Read available data into the end of our buffer
    // Ensure not to overflow receive_buffer_ beyond MAX_RECEIVE_BUFFER_SIZE
    size_t space_available_in_buffer = MAX_RECEIVE_BUFFER_SIZE - receive_buffer_watermark_;
    if (space_available_in_buffer == 0) {
        logger_.Error("TransportHandler [{}]: Receive buffer full ({} bytes). Cannot read more data. Possible parsing stall or message too large.", gateway_id_, MAX_RECEIVE_BUFFER_SIZE);
        // This is a critical situation. Options:
        // 1. Disconnect and report error.
        // 2. Clear buffer and try to resync (risky).
        // For now, log and stop reading to prevent further issues. The connection might eventually break.
        notify_observer_critical_error("Receive buffer overflow");
        // To prevent busy loop, perhaps disconnect
        // connected_status_.store(false);
        // tcp_client_->disconnect();
        return;
    }

    int bytes_received = tcp_client_->receive_data(receive_buffer_.data() + receive_buffer_watermark_, space_available_in_buffer);

    if (bytes_received > 0) {
        receive_buffer_watermark_ += bytes_received;
        logger_.Debug("TransportHandler [{}]: Received {} bytes. Buffer watermark: {}", gateway_id_, bytes_received, receive_buffer_watermark_);

        // Process all complete messages in the buffer
        while (!shutdown_requested_.load()) {
            if (receive_buffer_watermark_ < ModGW::Header::MIN_HEADER_LEN_BEFORE_TOPIC_NAME) {
                // Not enough data for even the minimal header
                break; 
            }

            std::optional<ParsedHeaderInfo> header_info = parse_message_header(receive_buffer_.data(), receive_buffer_watermark_);
            
            if (!header_info) {
                // This case should ideally be caught by the MIN_HEADER_LEN_BEFORE_TOPIC_NAME check above
                // if parse_modular_gw_header returns nullopt for insufficient data for fixed part.
                // If it returns nullopt for other reasons (e.g. bad magic number with enough bytes),
                // we might have a desync.
                logger_.Error("TransportHandler [{}]: Failed to parse message header from buffered data (watermark: {}). Possible data corruption or desync.", gateway_id_, receive_buffer_watermark_);
                // Desync handling: clear buffer and hope for the best, or disconnect.
                // For now, clearing buffer to try to recover. This is a simplistic approach.
                receive_buffer_watermark_ = 0; 
                notify_observer_critical_error("Header parsing failed, buffer cleared");
                break; 
            }

            size_t total_expected_msg_len = header_info->total_message_length;

            if (receive_buffer_watermark_ >= total_expected_msg_len) {
                // We have a complete message
                logger_.Debug("TransportHandler [{}]: Complete message received (Total: {} bytes). Routing...", gateway_id_, total_expected_msg_len);
                
                auto msg_to_route = std::make_shared<Message>();
                msg_to_route->data.assign(receive_buffer_.data(), receive_buffer_.data() + total_expected_msg_len);

                auto destinations = routing_table_->get_destinations(header_info->routing_key);
                if (destinations.empty()) {
                    logger_.Warn("TransportHandler [{}]: No route found for SourceID: {}, MsgType: {}", 
                             gateway_id_, header_info->routing_key.source_id, header_info->routing_key.message_type);
                } else {
                    for (const auto& dest_queue : destinations) {
                        if (dest_queue) { // Ensure queue pointer is valid
                            if (!dest_queue->enqueue(msg_to_route)) { // MPSC queue enqueue
                                logger_.Error("TransportHandler [{}]: Failed to enqueue message to destination queue.", gateway_id_);
                                // Potentially notify CP or handle queue full scenario
                            } else {
                                logger_.Debug("TransportHandler [{}]: Enqueued message for SourceID: {}, MsgType: {}", 
                                          gateway_id_, header_info->routing_key.source_id, header_info->routing_key.message_type);
                            }
                        }
                    }
                }

                // Remove processed message from buffer by shifting remaining data
                if (receive_buffer_watermark_ > total_expected_msg_len) {
                    std::memmove(receive_buffer_.data(), receive_buffer_.data() + total_expected_msg_len, receive_buffer_watermark_ - total_expected_msg_len);
                }
                receive_buffer_watermark_ -= total_expected_msg_len;
                logger_.Debug("TransportHandler [{}]: Processed message. New buffer watermark: {}", gateway_id_, receive_buffer_watermark_);

            } else {
                // Not enough data for a complete message yet, need to read more
                logger_.Debug("TransportHandler [{}]: Incomplete message. Expected: {}, Have: {}. Waiting for more data.", gateway_id_, total_expected_msg_len, receive_buffer_watermark_);
                break; 
            }
        }
    } else if (bytes_received == 0) {
        // Peer closed connection gracefully
        logger_.Info("TransportHandler [{}]: Connection closed by peer (received 0 bytes).", gateway_id_);
        connected_status_.store(false);
        notify_observer_disconnected("Peer closed connection");
        if(tcp_client_) tcp_client_->disconnect(); // Ensure our side is also closed
    } else { // bytes_received < 0
        // Error during receive
        logger_.Error("TransportHandler [{}]: Receive error: {} (errno: {}). Disconnecting.", gateway_id_, strerror(errno), errno);
        connected_status_.store(false);
        notify_observer_disconnected("Receive error");
        if(tcp_client_) tcp_client_->disconnect();
    }
}

bool TransportHandler::handle_outgoing_messages() {
    if (!tcp_client_ || !tcp_client_->is_connected()) {
        // Don't attempt to send if not connected
        return false;
    }
    bool work_has_been_done = false;
    std::shared_ptr<Message> msg_to_send;
    // Try to dequeue without blocking indefinitely.
    // moodycamel::ConcurrentQueue's try_dequeue is non-blocking.
    if (input_queue_->try_dequeue(msg_to_send)) {
        work_has_been_done = true;
        if (msg_to_send && !msg_to_send->data.empty()) {
            logger_.Debug("TransportHandler [{}]: Dequeued message of size {} to send.", gateway_id_, msg_to_send->data.size());
            if (!tcp_client_->send_data(msg_to_send->data.data(), msg_to_send->data.size())) {
                logger_.Error("TransportHandler [{}]: Failed to send message of size {}. Error: {}. Disconnecting.", 
                          gateway_id_, msg_to_send->data.size(), strerror(errno));
                connected_status_.store(false);
                notify_observer_disconnected("Send error");
                if(tcp_client_) tcp_client_->disconnect();
            } else {
                logger_.Debug("TransportHandler [{}]: Successfully sent message of size {}.", gateway_id_, msg_to_send->data.size());
            }
        } else {
            logger_.Warn("TransportHandler [{}]: Dequeued null or empty message from input queue.", gateway_id_);
        }
    }

    // If try_dequeue fails, it means queue is empty, so just return and try later.
    return work_has_been_done;
}


// --- Placeholder Notification Methods ---
void TransportHandler::notify_observer_connected() {
    if (auto observer = cp_observer_weak_.lock()) {
        // observer->onHandlerConnected(gateway_id_); // Uncomment when ready
        logger_.Debug("TransportHandler [{}]: Placeholder: Would notify observer of connection.", gateway_id_);
    }
}

void TransportHandler::notify_observer_disconnected(const std::string& reason) {
    if (auto observer = cp_observer_weak_.lock()) {
        // observer->onHandlerDisconnected(gateway_id_, reason); // Uncomment when ready
        logger_.Debug("TransportHandler [{}]: Placeholder: Would notify observer of disconnection. Reason: {}", gateway_id_, reason);
    }
}

void TransportHandler::notify_observer_critical_error(const std::string& error_message) {
    if (auto observer = cp_observer_weak_.lock()) {
        // observer->onHandlerCriticalError(gateway_id_, error_message); // Uncomment when ready
        logger_.Debug("TransportHandler [{}]: Placeholder: Would notify observer of critical error: {}", gateway_id_, error_message);
    }
}