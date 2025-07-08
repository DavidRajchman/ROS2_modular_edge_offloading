#include "modular_gateway_sender/bridge_cp_client.hpp"
#include "modular_gateway_sender/transport/tcp_client_transport.hpp"
#include <vector>

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
        if (transport_ && transport_->data_available(1000)) { // Check for data with a 1s timeout
            std::vector<uint8_t> buffer(4096); // Buffer for incoming data
            int bytes_received = transport_->receive_data(buffer.data(), buffer.size());
            if (bytes_received > 0) {
                try {
                    json received_json = json::parse(buffer.begin(), buffer.begin() + bytes_received);
                    handle_received_message(received_json);
                } catch (const json::parse_error& e) {
                    logger_.Error("bridge_cp_client.cpp: JSON parse error: {}", e.what());
                }
            }
        }
        // Periodically check for messages that need to be re-sent
        check_for_timeouts();
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
            if (on_session_approved_) on_session_approved_(msg["payload"]["request_id"]);
            break;
        case 201: // SESSION_DENIED
            if (on_session_denied_) on_session_denied_(msg["payload"]["request_id"], msg["payload"]["reason_description"]);
            break;
        case 202: // DP_CONNECTION_CONFIRMED
            if (on_dp_confirmed_) on_dp_confirmed_();
            break;
        case 900: // ACK
        {
            int ack_seq_num = msg["payload"]["ack_sequence_number"];
            std::lock_guard<std::mutex> lock(pending_acks_mutex_);
            if (pending_acks_.erase(ack_seq_num)) {
                logger_.Info("bridge_cp_client.cpp: Received ACK for sequence number {}", ack_seq_num);
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
    for (auto const& [seq_num, pending] : pending_acks_) {
        if (now - pending.time_sent > ack_timeout_) {
            logger_.Warn("bridge_cp_client.cpp: ACK timeout for sequence number {}. Re-sending.", seq_num);
            std::string msg_str = pending.message.dump();
            std::vector<uint8_t> data(msg_str.begin(), msg_str.end());
            transport_->async_send_data(std::move(data));
            // Note: In a real implementation, you'd update the time_sent or add a retry counter.
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

    {
        std::lock_guard<std::mutex> lock(pending_acks_mutex_);
        pending_acks_[seq_num] = {msg, std::chrono::steady_clock::now()};
    }

    logger_.Info("bridge_cp_client.cpp: Sending reliable message: {}", msg.dump());
    std::string msg_str = msg.dump();
    std::vector<uint8_t> data(msg_str.begin(), msg_str.end());
    transport_->async_send_data(std::move(data));
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
    transport_->async_send_data(std::move(data));
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

void BridgeCpClient::send_offload_request(const std::string& component_id, const std::string& request_id, const std::string& task_id, const std::string& task_name) {
    json msg = {
        {"component_id", component_id},
        {"message_code", 100},
        {"message_type", "OFFLOAD_REQUEST"},
        {"payload", {
            {"request_id", request_id},
            {"task_id", task_id},
            {"task_name", task_name}
        }}
    };
    send_reliable_message(msg);
}

void BridgeCpClient::send_session_keepalive(const std::string& component_id, const std::string& request_id) {
    json msg = {
        {"component_id", component_id},
        {"message_code", 101},
        {"message_type", "SESSION_KEEPALIVE"},
        {"payload", {
            {"request_id", request_id}
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
            {"request_id", request_id}
        }}
    };
    send_reliable_message(msg);
}

} // namespace gateway