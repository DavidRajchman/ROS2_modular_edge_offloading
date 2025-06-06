#include "discovery_protocol/protocol.hpp"
#include <sstream>
#include <cstring>

namespace discovery_protocol {

// Constants
constexpr const char* MAGIC_STRING = "DISC:";
constexpr const char* MSG_TYPE_REG = "REG";
constexpr const char* MSG_TYPE_ACK = "ACK";
constexpr const char* MSG_TYPE_ERR = "ERR";
constexpr const char* MSG_TYPE_PNG = "PNG";
constexpr const char* MSG_TYPE_PON = "PON";

// Helper functions implementation
std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

const char* protocol_status_to_string(ProtocolStatus status) {
    switch (status) {
        case ProtocolStatus::OK: return "OK";
        case ProtocolStatus::BUFFER_TOO_SMALL: return "Buffer too small";
        case ProtocolStatus::INVALID_MESSAGE_TYPE: return "Invalid message type";
        case ProtocolStatus::MALFORMED_MESSAGE: return "Malformed message";
        case ProtocolStatus::MISSING_REQUIRED_FIELDS: return "Missing required fields";
        case ProtocolStatus::INVALID_FORMAT: return "Invalid format";
        case ProtocolStatus::FIELD_TOO_LONG: return "Field too long";
        case ProtocolStatus::INCOMPLETE_MESSAGE: return "Incomplete message";
        default: return "Unknown status";
    }
}

std::string component_type_to_string(ComponentType type) {
    switch (type) {
        case ComponentType::VEHICLE: return "V";
        case ComponentType::MEC: return "M";
        case ComponentType::BRIDGE: return "B";
        case ComponentType::OFFLOAD_MANAGER: return "O";
        case ComponentType::TEST: return "T";
        default: return "?";
    }
}

ComponentType string_to_component_type(const std::string& str) {
    if (str == "V") return ComponentType::VEHICLE;
    if (str == "M") return ComponentType::MEC;
    if (str == "B") return ComponentType::BRIDGE;
    if (str == "O") return ComponentType::OFFLOAD_MANAGER;
    if (str == "T") return ComponentType::TEST;
    // Default to TEST for unknown types
    return ComponentType::TEST;
}

std::string response_code_to_string(ResponseCode code) {
    switch (code) {
        case ResponseCode::SUCCESS: return "0";
        case ResponseCode::WAIT: return "W";
        case ResponseCode::ID_CONFLICT: return "C";
        case ResponseCode::INVALID_REQUEST: return "I";
        case ResponseCode::GENERAL_ERROR: return "E";
        default: return "E"; // Default to general error
    }
}

ResponseCode string_to_response_code(const std::string& str) {
    if (str == "0") return ResponseCode::SUCCESS;
    if (str == "W") return ResponseCode::WAIT;
    if (str == "C") return ResponseCode::ID_CONFLICT;
    if (str == "I") return ResponseCode::INVALID_REQUEST;
    return ResponseCode::GENERAL_ERROR; // Default to general error
}

std::string error_code_to_string(ErrorCode code) {
    switch (code) {
        case ErrorCode::CONFIGURATION: return "C";
        case ErrorCode::NETWORK: return "N";
        case ErrorCode::INTERNAL: return "I";
        case ErrorCode::PROTOCOL: return "P";
        default: return "I"; // Default to internal error
    }
}

ErrorCode string_to_error_code(const std::string& str) {
    if (str == "C") return ErrorCode::CONFIGURATION;
    if (str == "N") return ErrorCode::NETWORK;
    if (str == "P") return ErrorCode::PROTOCOL;
    return ErrorCode::INTERNAL; // Default to internal error
}

// Encode functions
ProtocolStatus encode_registration_request(const RegistrationRequest& request, std::string& output) {
    std::ostringstream oss;
    
    // Format: DISC:REG;V;S;2;1;VHC1_TEST;192.168.1.100:8080;Registration request from Vehicle
    oss << MAGIC_STRING << MSG_TYPE_REG << ";";
    oss << component_type_to_string(request.componentType) << ";";
    
    // ID request type (A/S)
    if (request.idRequestType == IdRequestType::AUTOMATIC) {
        oss << "A;;";  // No group ID or ID in group for automatic
    } else {
        oss << "S;" << static_cast<int>(request.groupId) << ";" << static_cast<int>(request.idInGroup) << ";";
    }
    
    oss << request.componentName << ";";
    oss << request.listenAddress << ":" << request.listenPort << ";";
    oss << request.humanReadableMessage;
    
    output = oss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_registration_request(const std::string& input, RegistrationRequest& output) {
    // Check if message starts with the magic string and message type
    if (input.substr(0, std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_REG)) != std::string(MAGIC_STRING) + MSG_TYPE_REG) {
        return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
    
    // Split the message by semicolons
    std::vector<std::string> parts = split_string(input.substr(std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_REG)), ';');
    
    // Ensure we have enough parts for a valid registration request
    // Format: Type;ReqType;GroupID;IDInGroup;Name;Address:Port;Message
    if (parts.size() < 6) {
        return ProtocolStatus::MISSING_REQUIRED_FIELDS;
    }
    
    // Parse component type
    if (parts[0].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    output.componentType = string_to_component_type(parts[0]);
    
    // Parse ID request type
    if (parts[1].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    output.idRequestType = (parts[1] == "A") ? IdRequestType::AUTOMATIC : IdRequestType::STATIC;
    
    // Parse group ID and ID in group if static ID
    if (output.idRequestType == IdRequestType::STATIC) {
        if (parts[2].empty() || parts[3].empty()) {
            return ProtocolStatus::MALFORMED_MESSAGE;
        }
        try {
            output.groupId = static_cast<uint8_t>(std::stoi(parts[2]));
            output.idInGroup = static_cast<uint8_t>(std::stoi(parts[3]));
        } catch (const std::exception&) {
            return ProtocolStatus::INVALID_FORMAT;
        }
    }
    
    // Parse component name
    output.componentName = parts[4];
    
    // Parse listen address and port
    if (parts[5].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    
    size_t colon_pos = parts[5].find(':');
    if (colon_pos == std::string::npos) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    
    output.listenAddress = parts[5].substr(0, colon_pos);
    output.listenPort = parts[5].substr(colon_pos + 1);
    
    // Parse human-readable message (optional)
    if (parts.size() > 6 && !parts[6].empty()) {
        output.humanReadableMessage = parts[6];
    } else {
        output.humanReadableMessage = "";
    }
    
    return ProtocolStatus::OK;
}

ProtocolStatus encode_registration_response(const RegistrationResponse& response, std::string& output) {
    std::ostringstream oss;
    
    // Format: DISC:ACK;0;2;1;B;192.168.1.15:9090;513;{"bridge_ip":"192.168.1.15"};Registration successful
    oss << MAGIC_STRING << MSG_TYPE_ACK << ";";
    oss << response_code_to_string(response.responseCode) << ";";
    oss << static_cast<int>(response.assignedGroupId) << ";";
    oss << static_cast<int>(response.assignedIdInGroup) << ";";
    
    // Add connection target information if available
    if (!response.connectionTargetType.empty() && !response.connectionTargetAddress.empty()) {
        oss << response.connectionTargetType << ";";
        oss << response.connectionTargetAddress << ":" << response.connectionTargetPort << ";";
        oss << response.connectionTargetId << ";";
    } else {
        oss << ";;;";  // Empty fields for no target
    }
    
    oss << response.configJson << ";";
    oss << response.humanReadableMessage;
    
    output = oss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_registration_response(const std::string& input, RegistrationResponse& output) {
    // Check if message starts with the magic string and message type
    if (input.substr(0, std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_ACK)) != std::string(MAGIC_STRING) + MSG_TYPE_ACK) {
        return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
    
    // Split the message by semicolons
    std::vector<std::string> parts = split_string(input.substr(std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_ACK)), ';');
    
    // Ensure we have enough parts for a valid registration response
    // Format: ResponseCode;GroupID;IDInGroup;TargetType;TargetAddress:Port;TargetID;ConfigJSON;Message
    if (parts.size() < 7) {
        return ProtocolStatus::MISSING_REQUIRED_FIELDS;
    }
    
    // Parse response code
    if (parts[0].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    output.responseCode = string_to_response_code(parts[0]);
    
    // Parse assigned group ID and ID in group
    if (parts[1].empty() || parts[2].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    
    try {
        output.assignedGroupId = static_cast<uint8_t>(std::stoi(parts[1]));
        output.assignedIdInGroup = static_cast<uint8_t>(std::stoi(parts[2]));
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    
    // Parse connection target information if available
    output.connectionTargetType = parts[3];
    
    if (!parts[4].empty()) {
        size_t colon_pos = parts[4].find(':');
        if (colon_pos == std::string::npos) {
            return ProtocolStatus::MALFORMED_MESSAGE;
        }
        
        output.connectionTargetAddress = parts[4].substr(0, colon_pos);
        output.connectionTargetPort = parts[4].substr(colon_pos + 1);
    } else {
        output.connectionTargetAddress = "";
        output.connectionTargetPort = "";
    }
    
    if (!parts[5].empty()) {
        try {
            output.connectionTargetId = static_cast<uint16_t>(std::stoi(parts[5]));
        } catch (const std::exception&) {
            return ProtocolStatus::INVALID_FORMAT;
        }
    } else {
        output.connectionTargetId = 0;
    }
    
    // Parse config JSON
    output.configJson = parts[6];
    
    // Parse human-readable message (optional)
    if (parts.size() > 7 && !parts[7].empty()) {
        output.humanReadableMessage = parts[7];
    } else {
        output.humanReadableMessage = "";
    }
    
    return ProtocolStatus::OK;
}

ProtocolStatus encode_keepalive_ping(const KeepalivePing& ping, std::string& output) {
    std::ostringstream oss;
    
    // Format: DISC:PNG;2.1;OK;;Keepalive ping from Vehicle 2.1
    oss << MAGIC_STRING << MSG_TYPE_PNG << ";";
    oss << ping.componentId << ";";
    oss << ping.status << ";;";  // Extra semicolon for compatibility
    oss << ping.humanReadableMessage;
    
    output = oss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_keepalive_ping(const std::string& input, KeepalivePing& output) {
    // Check if message starts with the magic string and message type
    if (input.substr(0, std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_PNG)) != std::string(MAGIC_STRING) + MSG_TYPE_PNG) {
        return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
    
    // Split the message by semicolons
    std::vector<std::string> parts = split_string(input.substr(std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_PNG)), ';');
    
    // Ensure we have enough parts for a valid keepalive ping
    // Format: ComponentID;Status;;Message
    if (parts.size() < 3) {
        return ProtocolStatus::MISSING_REQUIRED_FIELDS;
    }
    
    // Parse component ID
    if (parts[0].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    output.componentId = parts[0];
    
    // Parse status
    if (parts[1].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    output.status = parts[1];
    
    // Parse human-readable message (optional)
    if (parts.size() > 3 && !parts[3].empty()) {
        output.humanReadableMessage = parts[3];
    } else {
        output.humanReadableMessage = "";
    }
    
    return ProtocolStatus::OK;
}

ProtocolStatus encode_keepalive_response(const KeepaliveResponse& response, std::string& output) {
    std::ostringstream oss;
    
    // Format: DISC:PON;OK;5000;;Continue normal operation, next ping in 5 seconds
    oss << MAGIC_STRING << MSG_TYPE_PON << ";";
    oss << response.response << ";";
    oss << response.nextIntervalMs << ";;";  // Extra semicolon for compatibility
    oss << response.humanReadableMessage;
    
    output = oss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_keepalive_response(const std::string& input, KeepaliveResponse& output) {
    // Check if message starts with the magic string and message type
    if (input.substr(0, std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_PON)) != std::string(MAGIC_STRING) + MSG_TYPE_PON) {
        return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
    
    // Split the message by semicolons
    std::vector<std::string> parts = split_string(input.substr(std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_PON)), ';');
    
    // Ensure we have enough parts for a valid keepalive response
    // Format: Response;NextInterval;;Message
    if (parts.size() < 3) {
        return ProtocolStatus::MISSING_REQUIRED_FIELDS;
    }
    
    // Parse response
    if (parts[0].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    output.response = parts[0];
    
    // Parse next interval
    if (parts[1].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    
    try {
        output.nextIntervalMs = std::stoi(parts[1]);
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    
    // Parse human-readable message (optional)
    if (parts.size() > 3 && !parts[3].empty()) {
        output.humanReadableMessage = parts[3];
    } else {
        output.humanReadableMessage = "";
    }
    
    return ProtocolStatus::OK;
}

ProtocolStatus encode_error_message(const ErrorMessage& error, std::string& output) {
    std::ostringstream oss;
    
    // Format: DISC:ERR;C;2;1;;;;ID Conflict: Another component is already using ID 2.1
    oss << MAGIC_STRING << MSG_TYPE_ERR << ";";
    oss << error_code_to_string(error.errorCode) << ";;";  // Extra semicolon for compatibility
    oss << error.humanReadableMessage;
    
    output = oss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_error_message(const std::string& input, ErrorMessage& output) {
    // Check if message starts with the magic string and message type
    if (input.substr(0, std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_ERR)) != std::string(MAGIC_STRING) + MSG_TYPE_ERR) {
        return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
    
    // Split the message by semicolons
    std::vector<std::string> parts = split_string(input.substr(std::strlen(MAGIC_STRING) + std::strlen(MSG_TYPE_ERR)), ';');
    
    // Ensure we have enough parts for a valid error message
    // Format: ErrorCode;;Message
    if (parts.size() < 2) {
        return ProtocolStatus::MISSING_REQUIRED_FIELDS;
    }
    
    // Parse error code
    if (parts[0].empty()) {
        return ProtocolStatus::MALFORMED_MESSAGE;
    }
    output.errorCode = string_to_error_code(parts[0]);
    
    // Parse human-readable message (optional)
    if (parts.size() > 2 && !parts[2].empty()) {
        output.humanReadableMessage = parts[2];
    } else {
        output.humanReadableMessage = "";
    }
    
    return ProtocolStatus::OK;
}

ProtocolStatus encode_message(const Message& message, std::string& output) {
    switch (message.type) {
        case MessageType::REGISTRATION_REQUEST:
            return encode_registration_request(message.registrationRequest, output);
        case MessageType::REGISTRATION_RESPONSE:
            return encode_registration_response(message.registrationResponse, output);
        case MessageType::KEEPALIVE_PING:
            return encode_keepalive_ping(message.keepalivePing, output);
        case MessageType::KEEPALIVE_RESPONSE:
            return encode_keepalive_response(message.keepaliveResponse, output);
        case MessageType::ERROR:
            return encode_error_message(message.errorMessage, output);
        default:
            return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
}

ProtocolStatus decode_message(const std::string& input, Message& output) {
    // Check if the input is long enough to contain the magic string
    if (input.size() < std::strlen(MAGIC_STRING)) {
        return ProtocolStatus::INCOMPLETE_MESSAGE;
    }
    
    // Check magic string
    if (input.substr(0, std::strlen(MAGIC_STRING)) != MAGIC_STRING) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    
    // Check if the input is long enough to contain the message type
    if (input.size() < std::strlen(MAGIC_STRING) + 3) {
        return ProtocolStatus::INCOMPLETE_MESSAGE;
    }
    
    // Extract message type
    std::string msg_type = input.substr(std::strlen(MAGIC_STRING), 3);
    
    if (msg_type == MSG_TYPE_REG) {
        output.type = MessageType::REGISTRATION_REQUEST;
        return decode_registration_request(input, output.registrationRequest);
    } else if (msg_type == MSG_TYPE_ACK) {
        output.type = MessageType::REGISTRATION_RESPONSE;
        return decode_registration_response(input, output.registrationResponse);
    } else if (msg_type == MSG_TYPE_PNG) {
        output.type = MessageType::KEEPALIVE_PING;
        return decode_keepalive_ping(input, output.keepalivePing);
    } else if (msg_type == MSG_TYPE_PON) {
        output.type = MessageType::KEEPALIVE_RESPONSE;
        return decode_keepalive_response(input, output.keepaliveResponse);
    } else if (msg_type == MSG_TYPE_ERR) {
        output.type = MessageType::ERROR;
        return decode_error_message(input, output.errorMessage);
    } else {
        return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
}

} // namespace discovery_protocol