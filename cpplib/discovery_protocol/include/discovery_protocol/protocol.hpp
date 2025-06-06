#ifndef DISCOVERY_PROTOCOL_HPP
#define DISCOVERY_PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

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

// Component types
enum class ComponentType {
    VEHICLE,        // "V"
    MEC,            // "M"
    BRIDGE,         // "B"
    OFFLOAD_MANAGER, // "O"
    TEST            // "T"
};

// ID request types
enum class IdRequestType {
    AUTOMATIC,      // "A"
    STATIC          // "S"
};

// Response codes
enum class ResponseCode {
    SUCCESS,        // "0"
    WAIT,           // "W"
    ID_CONFLICT,    // "C"
    INVALID_REQUEST, // "I"
    GENERAL_ERROR   // "E"
};

// Message types
enum class MessageType {
    REGISTRATION_REQUEST,   // "REG"
    REGISTRATION_RESPONSE,  // "ACK"
    ERROR,                  // "ERR"
    KEEPALIVE_PING,         // "PNG"
    KEEPALIVE_RESPONSE      // "PON"
};

// Error codes
enum class ErrorCode {
    CONFIGURATION,  // "C"
    NETWORK,        // "N"
    INTERNAL,       // "I"
    PROTOCOL        // "P"
};

// Registration request message
struct RegistrationRequest {
    ComponentType componentType;
    IdRequestType idRequestType;
    uint8_t groupId;            // Only used when idRequestType is STATIC
    uint8_t idInGroup;          // Only used when idRequestType is STATIC
    std::string componentName;
    std::string listenAddress;
    std::string listenPort;
    std::string humanReadableMessage;
};

// Registration response message
struct RegistrationResponse {
    ResponseCode responseCode;
    uint8_t assignedGroupId;
    uint8_t assignedIdInGroup;
    std::string connectionTargetType;     // Empty if no target
    std::string connectionTargetAddress;  // Empty if no target
    std::string connectionTargetPort;     // Empty if no target
    uint16_t connectionTargetId;          // 0 if no target
    std::string configJson;
    std::string humanReadableMessage;
};

// Keepalive ping message
struct KeepalivePing {
    std::string componentId;    // Format: "groupId.idInGroup" (e.g. "2.1")
    std::string status;         // "OK" or "ERR"
    std::string humanReadableMessage;
};

// Keepalive response message
struct KeepaliveResponse {
    std::string response;       // "OK", "UPD" (update needed), or "DIS" (disconnect)
    int nextIntervalMs;        // Next ping interval in milliseconds
    std::string humanReadableMessage;
};

// Error message
struct ErrorMessage {
    ErrorCode errorCode;
    std::string humanReadableMessage;
};

// Message base class to hold any message type
struct Message {
    MessageType type;
    
    union {
        RegistrationRequest registrationRequest;
        RegistrationResponse registrationResponse;
        KeepalivePing keepalivePing;
        KeepaliveResponse keepaliveResponse;
        ErrorMessage errorMessage;
    };
    
    // Constructors for union initialization
    Message() : type(MessageType::ERROR) {}
    explicit Message(const RegistrationRequest& req) : type(MessageType::REGISTRATION_REQUEST), registrationRequest(req) {}
    explicit Message(const RegistrationResponse& res) : type(MessageType::REGISTRATION_RESPONSE), registrationResponse(res) {}
    explicit Message(const KeepalivePing& ping) : type(MessageType::KEEPALIVE_PING), keepalivePing(ping) {}
    explicit Message(const KeepaliveResponse& pong) : type(MessageType::KEEPALIVE_RESPONSE), keepaliveResponse(pong) {}
    explicit Message(const ErrorMessage& err) : type(MessageType::ERROR), errorMessage(err) {}
};

// Encode a message to string
ProtocolStatus encode_message(const Message& message, std::string& output);

// Decode a string to a message
ProtocolStatus decode_message(const std::string& input, Message& output);

// Helper functions for specific message types
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

// Helper functions
const char* protocol_status_to_string(ProtocolStatus status);
std::string component_type_to_string(ComponentType type);
ComponentType string_to_component_type(const std::string& str);
std::string response_code_to_string(ResponseCode code);
ResponseCode string_to_response_code(const std::string& str);
std::string error_code_to_string(ErrorCode code);
ErrorCode string_to_error_code(const std::string& str);

// Split a string by delimiter
std::vector<std::string> split_string(const std::string& str, char delimiter);

} // namespace discovery_protocol

#endif // DISCOVERY_PROTOCOL_HPP