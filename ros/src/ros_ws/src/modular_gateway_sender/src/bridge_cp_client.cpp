#include "modular_gateway_sender/bridge_cp_client.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include <vector>
#include <thread>
#include <chrono>

namespace gateway {

BridgeCpClient::BridgeCpClient() 
    : logger_(CppLogging::Logger("gateway")) 
{
    logger_.Info("bridge_cp_client.cpp: BridgeCpClient constructed.");
}

BridgeCpClient::~BridgeCpClient() {
    logger_.Info("bridge_cp_client.cpp: BridgeCpClient destructed.");
    stop();
}

bool BridgeCpClient::start(
    const std::string& host, 
    int port,
    DpConfirmedCallback dp_confirmed_cb,
    SessionApprovedCallback session_approved_cb,
    SessionDeniedCallback session_denied_cb)
{
    if (running_) {
        logger_.Warn("bridge_cp_client.cpp: Client already running.");
        return true;
    }

    on_dp_confirmed_ = dp_confirmed_cb;
    on_session_approved_ = session_approved_cb;
    on_session_denied_ = session_denied_cb;

    transport_ = std::make_unique<TcpClientTransport>(host, port);
    if (!transport_->connect()) {
        logger_.Error("bridge_cp_client.cpp: Failed to connect to Bridge Control Plane at {}:{}", host, port);
        return false;
    }

    running_ = true;
    client_thread_ = std::thread(&BridgeCpClient::client_thread_func, this);
    logger_.Info("bridge_cp_client.cpp: Bridge CP client started and connected to {}:{}", host, port);
    return true;
}

void BridgeCpClient::stop() {
    running_ = false;
    if (client_thread_.joinable()) {
        client_thread_.join();
    }
    if (transport_) {
        transport_->disconnect();
    }
    logger_.Info("bridge_cp_client.cpp: Bridge CP client stopped.");
}

void BridgeCpClient::client_thread_func() {
    while (running_) {
        bool received_data = false;
        
        if (transport_ && transport_->is_connected() && transport_->data_available(1000)) { // Check for data with a 1s timeout
            std::vector<uint8_t> buffer(4096); // Buffer for incoming data
            int bytes_received = transport_->receive_data(buffer.data(), buffer.size() - 1);

            if (bytes_received > 0) {
                buffer[bytes_received] = '\0'; // Null-terminate the received data
                std::string json_str(reinterpret_cast<char*>(buffer.data()));
                try {
                    json msg = json::parse(json_str);
                    handle_received_message(msg);
                    received_data = true;
                } catch (const json::parse_error& e) {
                    logger_.Error("bridge_cp_client.cpp: JSON parse error: {}. Received data: {}", e.what(), json_str);
                }
            } else if (bytes_received < 0) {
                // An error occurred, or the connection was closed. The transport handles logging this.
                // The loop will continue, and is_connected() will eventually be false.
            }
        }
        
        // Periodically check for messages that need to be re-sent
        check_for_timeouts();
        
        // If we didn't receive any data and transport is not connected, add a small delay
        // to prevent tight looping when connection is lost
        if (!received_data && (!transport_ || !transport_->is_connected())) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void BridgeCpClient::handle_received_message(const json& msg) {
    logger_.Info("bridge_cp_client.cpp: Received message: {}", msg.dump());

    if (!msg.contains("message_code") || !msg.contains("component_id")) {
        logger_.Error("bridge_cp_client.cpp: Received malformed message (missing code or component_id).");
        return;
    }

    int code = msg["message_code"];
    std::string component_id = msg["component_id"];
    int seq_num = msg.value("sequence_number", 0);

    // Always send an ACK for any message that is not itself an ACK
    if (code != 900) {
        send_ack(seq_num, component_id);
    }

    switch (code) {
        case 200: // SESSION_APPROVED
            if (on_session_approved_ && msg.contains("payload")) {
                json payload_copy = msg["payload"]; // make a mutable copy we can normalize

                // Normalize request_id (may be number or string)
                if (payload_copy.contains("request_id")) {
                    try {
                        if (payload_copy["request_id"].is_string()) {
                            std::string request_id_str = payload_copy["request_id"].get<std::string>();
                            logger_.Info("bridge_cp_client.cpp: Processing SESSION_APPROVED for request_id '{}'", request_id_str);
                        } else if (payload_copy["request_id"].is_number_integer()) {
                            auto rid_val = payload_copy["request_id"].get<int64_t>();
                            std::string request_id_str = std::to_string(rid_val);
                            payload_copy["request_id"] = request_id_str; // replace with string for downstream code expecting string
                            logger_.Info("bridge_cp_client.cpp: Processing SESSION_APPROVED for numeric request_id {} (normalized to '{}')", rid_val, request_id_str);
                        } else {
                            logger_.Warn("bridge_cp_client.cpp: SESSION_APPROVED request_id has unexpected type (neither string nor integer)");
                        }
                    } catch (const std::exception& ex) {
                        logger_.Error("bridge_cp_client.cpp: Exception normalizing request_id: {}", ex.what());
                    }
                } else {
                    logger_.Info("bridge_cp_client.cpp: Processing SESSION_APPROVED without request_id (likely unsolicited for MEC)");
                }
                
                // Ensure task_id is present for MEC case and normalize
                if (payload_copy.contains("task_id")) {
                    try {
                        if (payload_copy["task_id"].is_number_integer()) {
                            auto tid_val = payload_copy["task_id"].get<int64_t>();
                            std::string task_id_str = std::to_string(tid_val);
                            payload_copy["task_id"] = task_id_str; // store as string for uniform handling downstream
                            logger_.Info("bridge_cp_client.cpp: Normalized numeric task_id {} to '{}'", tid_val, task_id_str);
                        } else if (payload_copy["task_id"].is_string()) {
                            logger_.Info("bridge_cp_client.cpp: task_id already string: '{}'", payload_copy["task_id"].get<std::string>());
                        } else {
                            logger_.Warn("bridge_cp_client.cpp: SESSION_APPROVED task_id has unexpected type");
                        }
                    } catch (const std::exception& ex) {
                        logger_.Error("bridge_cp_client.cpp: Exception normalizing task_id: {}", ex.what());
                    }
                } else {
                    logger_.Warn("bridge_cp_client.cpp: SESSION_APPROVED payload missing task_id - may cause issues for MEC components");
                }
                
                on_session_approved_(payload_copy);
            } else {
                logger_.Error("bridge_cp_client.cpp: Malformed SESSION_APPROVED message.");
            }
            break;
        case 201: // SESSION_DENIED
            if (on_session_denied_ && msg.contains("payload") && msg["payload"].contains("request_id") && msg["payload"].contains("reason_description")) {
                try {
                    std::string request_id_str;
                    const auto& rid = msg["payload"]["request_id"];
                    if (rid.is_string()) request_id_str = rid.get<std::string>();
                    else if (rid.is_number_integer()) request_id_str = std::to_string(rid.get<int64_t>());
                    else request_id_str = "";
                    on_session_denied_(request_id_str, msg["payload"]["reason_description"].get<std::string>());
                } catch (const std::exception& ex) {
                    logger_.Error("bridge_cp_client.cpp: Exception handling SESSION_DENIED: {}", ex.what());
                }
            } else {
                logger_.Error("bridge_cp_client.cpp: Malformed SESSION_DENIED message.");
            }
            break;
        case 202: // DP_CONNECTION_CONFIRMED
            if (on_dp_confirmed_) {
                on_dp_confirmed_();
            }
            break;
        case 900: // ACK
        {
            if (msg.contains("payload") && msg["payload"].contains("ack_sequence_number")) {
                int ack_seq_num = msg["payload"]["ack_sequence_number"];
                std::lock_guard<std::mutex> lock(pending_acks_mutex_);
                if (pending_acks_.erase(ack_seq_num)) {
                    logger_.Info("bridge_cp_client.cpp: Received ACK for sequence number {}", ack_seq_num);
                }
            } else {
                logger_.Error("bridge_cp_client.cpp: Malformed ACK message.");
            }
            break;
        }
        default:
            logger_.Warn("bridge_cp_client.cpp: Received message with unhandled code: {}", code);
            break;
    }
}

void BridgeCpClient::check_for_timeouts() {
    std::lock_guard<std::mutex> lock(pending_acks_mutex_);
    auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<uint64_t, json>> messages_to_resend; // Store seq_num with message for debugging

    //logger_.Debug("bridge_cp_client.cpp: Checking timeouts for {} pending messages", pending_acks_.size());
    
    for (auto const& [seq_num, pending] : pending_acks_) {
        auto time_elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - pending.time_sent);
        logger_.Debug("bridge_cp_client.cpp: Sequence {} elapsed time: {}s, timeout: {}s", 
                     seq_num, time_elapsed.count(), ack_timeout_.count());
        
        if (now - pending.time_sent > ack_timeout_) {
            logger_.Warn("bridge_cp_client.cpp: ACK timeout for sequence number {}. Resending.", seq_num);
            messages_to_resend.emplace_back(seq_num, pending.message);
        }
    }

    //logger_.Debug("bridge_cp_client.cpp: Found {} messages to resend", messages_to_resend.size());

    // Resend outside the loop to avoid iterator invalidation issues if we were modifying the map
    for (const auto& [seq_num, msg] : messages_to_resend) {
        logger_.Info("bridge_cp_client.cpp: Attempting to resend sequence number {}", seq_num);
        std::string msg_str = msg.dump();
        std::vector<uint8_t> data(msg_str.begin(), msg_str.end());
        TransportAsyncSendResult result = transport_->async_send_data(std::move(data));
        
        if (result == TransportAsyncSendResult::SUCCESS) {
            // Only update the time_sent if the resend was successful
            auto it = pending_acks_.find(seq_num);
            if (it != pending_acks_.end()) {
                it->second.time_sent = std::chrono::steady_clock::now();
                logger_.Debug("bridge_cp_client.cpp: Successfully resent message with sequence number {}", seq_num);
            }
        } else {
            // Log the specific failure reason
            const char* error_str = "UNKNOWN";
            switch (result) {
                case TransportAsyncSendResult::QUEUE_FULL: error_str = "QUEUE_FULL"; break;
                case TransportAsyncSendResult::NOT_CONNECTED: error_str = "NOT_CONNECTED"; break;
                case TransportAsyncSendResult::INTERNAL_ERROR: error_str = "INTERNAL_ERROR"; break;
                default: break;
            }
            logger_.Error("bridge_cp_client.cpp: Failed to resend message with sequence number {}: {}", seq_num, error_str);
            // Do NOT update time_sent so it will be retried again next timeout check
        }
    }
}

void BridgeCpClient::send_reliable_message(json& msg) {
    if (!running_ || !transport_ || !transport_->is_connected()) {
        logger_.Error("bridge_cp_client.cpp: Cannot send message, client not running or connected.");
        return;
    }

    uint64_t seq_num = sequence_number_++;
    msg["sequence_number"] = seq_num;

    logger_.Info("bridge_cp_client.cpp: Sending reliable message: {}", msg.dump());
    std::string msg_str = msg.dump();
    std::vector<uint8_t> data(msg_str.begin(), msg_str.end());
    TransportAsyncSendResult result = transport_->async_send_data(std::move(data));
    
    if (result == TransportAsyncSendResult::SUCCESS) {
        // Only add to pending_acks if the send was successful
        std::lock_guard<std::mutex> lock(pending_acks_mutex_);
        pending_acks_[seq_num] = {msg, std::chrono::steady_clock::now()};
        logger_.Debug("bridge_cp_client.cpp: Message with sequence number {} queued for ACK tracking", seq_num);
    } else {
        // Log the specific failure reason
        const char* error_str = "UNKNOWN";
        switch (result) {
            case TransportAsyncSendResult::QUEUE_FULL: error_str = "QUEUE_FULL"; break;
            case TransportAsyncSendResult::NOT_CONNECTED: error_str = "NOT_CONNECTED"; break;
            case TransportAsyncSendResult::INTERNAL_ERROR: error_str = "INTERNAL_ERROR"; break;
            default: break;
        }
        logger_.Error("bridge_cp_client.cpp: Failed to send message with sequence number {}: {}", seq_num, error_str);
        // Do not add to pending_acks since the message was not actually sent
    }
}

void BridgeCpClient::send_ack(int ack_sequence_number, const std::string& component_id) {
    json ack_msg = {
        {"component_id", component_id},
        {"message_code", 900},
        {"message_type", "ACK"},
        {"sequence_number", 0}, // ACKs don't need their own sequence number for reliability
        {"payload", {
            {"ack_sequence_number", ack_sequence_number}
        }}
    };
    logger_.Info("bridge_cp_client.cpp: Sending ACK for sequence number {}", ack_sequence_number);
    std::string msg_str = ack_msg.dump();
    std::vector<uint8_t> data(msg_str.begin(), msg_str.end());
    TransportAsyncSendResult result = transport_->async_send_data(std::move(data));
    
    if (result != TransportAsyncSendResult::SUCCESS) {
        const char* error_str = "UNKNOWN";
        switch (result) {
            case TransportAsyncSendResult::QUEUE_FULL: error_str = "QUEUE_FULL"; break;
            case TransportAsyncSendResult::NOT_CONNECTED: error_str = "NOT_CONNECTED"; break;
            case TransportAsyncSendResult::INTERNAL_ERROR: error_str = "INTERNAL_ERROR"; break;
            default: break;
        }
        logger_.Warn("bridge_cp_client.cpp: Failed to send ACK for sequence number {}: {}", ack_sequence_number, error_str);
    }
}

void BridgeCpClient::send_dp_info(const std::string& component_id, const std::string& dp_host, int dp_port) {
    json msg = {
        {"component_id", component_id},
        {"message_code", 103},
        {"message_type", "DP_INFO"},
        {"payload", {
            {"dp_host", dp_host},
            {"dp_port", dp_port}
        }}
    };
    send_reliable_message(msg);
}

void BridgeCpClient::send_offload_request(const std::string& component_id, const std::string& request_id, const std::string& task_id, const std::string& task_name, const std::string& vhc_data) {
    json payload = {
        {"request_id", request_id},  // Convert to numeric upstream if required
        {"task_id", std::stoi(task_id)},        // Convert to numeric for transport
        {"task_name", task_name}
    };
    
    // Only add vhc_data if it's not empty
    if (!vhc_data.empty()) {
        payload["vhc_data"] = vhc_data;
        logger_.Info("bridge_cp_client.cpp: Including vhc_data in offload request: '{}'", vhc_data);
    }
    
    json msg = {
        {"component_id", component_id},
        {"message_code", 100},
        {"message_type", "OFFLOAD_REQUEST"},
        {"payload", payload}
    };
    send_reliable_message(msg);
}

void BridgeCpClient::send_session_keepalive(const std::string& component_id, const std::string& request_id) {
    json msg = {
        {"component_id", component_id},
        {"message_code", 101},
        {"message_type", "SESSION_KEEPALIVE"},
        {"payload", {
            {"request_id", request_id}  // Convert to numeric upstream if required
        }}
    };
    send_reliable_message(msg);
}

void BridgeCpClient::send_session_terminate_request(const std::string& component_id, const std::string& request_id) {
    json msg = {
        {"component_id", component_id},
        {"message_code", 102},
        {"message_type", "SESSION_TERMINATE_REQUEST"},
        {"payload", {
            {"request_id", request_id}  // Convert to numeric upstream if required
        }}
    };
    send_reliable_message(msg);
}

} // namespace gateway