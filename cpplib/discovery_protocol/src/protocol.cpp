#include "discovery_protocol/protocol.hpp"
#include <sstream>
#include <cstring>
#include <variant>

namespace discovery_protocol {

ProtocolStatus encode_message(const Message& message, std::string& output) {
    switch (message.type) {
        case MessageType::REGISTRATION_REQUEST:
            return encode_registration_request(std::get<RegistrationRequest>(message.data), output);
        case MessageType::REGISTRATION_RESPONSE:
            return encode_registration_response(std::get<RegistrationResponse>(message.data), output);
        case MessageType::KEEPALIVE_PING:
            return encode_keepalive_ping(std::get<KeepalivePing>(message.data), output);
        case MessageType::KEEPALIVE_RESPONSE:
            return encode_keepalive_response(std::get<KeepaliveResponse>(message.data), output);
        case MessageType::ERROR:
            return encode_error_message(std::get<ErrorMessage>(message.data), output);
        default:
            return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
}

ProtocolStatus decode_message(const std::string& input, Message& output) {
    if (input.size() < 5) {
        return ProtocolStatus::INCOMPLETE_MESSAGE;
    }
    if (input.substr(0, 5) != "DISC:") {
        return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
    if (input.size() < 8) {
        return ProtocolStatus::INCOMPLETE_MESSAGE;
    }
    std::string msg_type = input.substr(5, 3);
    if (msg_type == "REG") {
        RegistrationRequest req;
        ProtocolStatus status = decode_registration_request(input, req);
        if (status == ProtocolStatus::OK) {
            output.type = MessageType::REGISTRATION_REQUEST;
            output.data = req;
        }
        return status;
    } else if (msg_type == "ACK") {
        RegistrationResponse res;
        ProtocolStatus status = decode_registration_response(input, res);
        if (status == ProtocolStatus::OK) {
            output.type = MessageType::REGISTRATION_RESPONSE;
            output.data = res;
        }
        return status;
    } else if (msg_type == "PNG") {
        KeepalivePing ping;
        ProtocolStatus status = decode_keepalive_ping(input, ping);
        if (status == ProtocolStatus::OK) {
            output.type = MessageType::KEEPALIVE_PING;
            output.data = ping;
        }
        return status;
    } else if (msg_type == "PON") {
        KeepaliveResponse pong;
        ProtocolStatus status = decode_keepalive_response(input, pong);
        if (status == ProtocolStatus::OK) {
            output.type = MessageType::KEEPALIVE_RESPONSE;
            output.data = pong;
        }
        return status;
    } else if (msg_type == "ERR") {
        ErrorMessage err;
        ProtocolStatus status = decode_error_message(input, err);
        if (status == ProtocolStatus::OK) {
            output.type = MessageType::ERROR;
            output.data = err;
        }
        return status;
    }
    return ProtocolStatus::INVALID_MESSAGE_TYPE;
}

} // namespace discovery_protocol#include "discovery_protocol/protocol.hpp"
