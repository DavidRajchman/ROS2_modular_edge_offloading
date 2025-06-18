#ifndef DISCOVERY_PROTOCOL_HPP
#define DISCOVERY_PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <variant>

namespace discovery_protocol {

// Error codes for protocol operations
enum class ProtocolStatus {
    OK = 0,
    BUFFER_TOO_SMALL,
    INVALID_MESSAGE_TYPE,
    MALFORMED_MESSAGE,
    MISSING_REQUIRED_FIELDS,
    INVALID_FORMAT,
    FIELD_TOO_LONG,
    INCOMPLETE_MESSAGE
};

enum class ComponentType {
    VEHICLE,
    MEC,
    BRIDGE,
    OFFLOAD_MANAGER,
    TEST
};

enum class IdRequestType {
    AUTOMATIC,
    STATIC
};

enum class ResponseCode {
    SUCCESS,
    WAIT,
    ID_CONFLICT,
    INVALID_REQUEST,
    GENERAL_ERROR
};

enum class MessageType {
    REGISTRATION_REQUEST,
    REGISTRATION_RESPONSE,
    ERROR,
    KEEPALIVE_PING,
    KEEPALIVE_RESPONSE
};

enum class ErrorCode {
    CONFIGURATION,
    NETWORK,
    INTERNAL,
    PROTOCOL
};

struct RegistrationRequest {
    ComponentType componentType;
    IdRequestType idRequestType;
    uint8_t groupId;
    uint8_t idInGroup;
    std::string componentName;
    std::string listenAddress;
    std::string listenPort;
    std::string humanReadableMessage;
};

struct RegistrationResponse {
    ResponseCode responseCode;
    uint8_t assignedGroupId;
    uint8_t assignedIdInGroup;
    std::string connectionTargetType;
    std::string connectionTargetAddress;
    std::string connectionTargetPort;
    uint16_t connectionTargetId;
    std::string configJson;
    std::string humanReadableMessage;
};

struct KeepalivePing {
    std::string componentId;
    std::string status;
    std::string humanReadableMessage;
};

struct KeepaliveResponse {
    std::string response;
    int nextIntervalMs;
    std::string humanReadableMessage;
};

struct ErrorMessage {
    ErrorCode errorCode;
    std::string humanReadableMessage;
};

using MessageVariant = std::variant<
    RegistrationRequest,
    RegistrationResponse,
    ErrorMessage,
    KeepalivePing,
    KeepaliveResponse
>;

struct Message {
    MessageType type;
    MessageVariant data;

    Message() : type(MessageType::ERROR), data(ErrorMessage{}) {}
    explicit Message(const RegistrationRequest& req) : type(MessageType::REGISTRATION_REQUEST), data(req) {}
    explicit Message(const RegistrationResponse& res) : type(MessageType::REGISTRATION_RESPONSE), data(res) {}
    explicit Message(const KeepalivePing& ping) : type(MessageType::KEEPALIVE_PING), data(ping) {}
    explicit Message(const KeepaliveResponse& pong) : type(MessageType::KEEPALIVE_RESPONSE), data(pong) {}
    explicit Message(const ErrorMessage& err) : type(MessageType::ERROR), data(err) {}
};

ProtocolStatus encode_message(const Message& message, std::string& output);
ProtocolStatus decode_message(const std::string& input, Message& output);

ProtocolStatus encode_registration_request(const RegistrationRequest& request, std::string& output);
ProtocolStatus decode_registration_request(const std::string& input, RegistrationRequest& output);

ProtocolStatus encode_registration_response(const RegistrationResponse& response, std::string& output);
ProtocolStatus decode_registration_response(const std::string& input, RegistrationResponse& output);

ProtocolStatus encode_keepalive_ping(const KeepalivePing& ping, std::string& output);
ProtocolStatus decode_keepalive_ping(const std::string& input, KeepalivePing& output);

ProtocolStatus encode_keepalive_response(const KeepaliveResponse& response, std::string& output);
ProtocolStatus decode_keepalive_response(const std::string& input, KeepaliveResponse& output);

ProtocolStatus encode_error_message(const ErrorMessage& error, std::string& output);
ProtocolStatus decode_error_message(const std::string& input, ErrorMessage& output);

const char* protocol_status_to_string(ProtocolStatus status);
std::string component_type_to_string(ComponentType type);
ComponentType string_to_component_type(const std::string& str);
std::string response_code_to_string(ResponseCode code);
ResponseCode string_to_response_code(const std::string& str);
std::string error_code_to_string(ErrorCode code);
ErrorCode string_to_error_code(const std::string& str);

std::vector<std::string> split_string(const std::string& str, char delimiter);

} // namespace discovery_protocol

#endif // DISCOVERY_PROTOCOL_HPP