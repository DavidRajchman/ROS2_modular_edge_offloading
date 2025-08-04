#include "discovery_protocol/protocol.hpp"
#include <sstream>
#include <cstring>
#include <vector>
#include <algorithm>

namespace discovery_protocol {

// Helper function to split strings, useful for parsing
std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

// --- Enum to String Conversion ---

const char* protocol_status_to_string(ProtocolStatus status) {
    switch (status) {
        case ProtocolStatus::OK: return "OK";
        case ProtocolStatus::BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
        case ProtocolStatus::INVALID_MESSAGE_TYPE: return "INVALID_MESSAGE_TYPE";
        case ProtocolStatus::MALFORMED_MESSAGE: return "MALFORMED_MESSAGE";
        case ProtocolStatus::MISSING_REQUIRED_FIELDS: return "MISSING_REQUIRED_FIELDS";
        case ProtocolStatus::INVALID_FORMAT: return "INVALID_FORMAT";
        case ProtocolStatus::FIELD_TOO_LONG: return "FIELD_TOO_LONG";
        case ProtocolStatus::INCOMPLETE_MESSAGE: return "INCOMPLETE_MESSAGE";
        default: return "UNKNOWN_STATUS";
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
    return ComponentType::TEST; // Default/unknown
}

// --- Message-level encode/decode ---

PProtocolStatus encode_message(const Message& message, std::string& output) {
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
        case MessageType::COMPONENT_QUERY:  // NEW
            return encode_component_query(std::get<ComponentQuery>(message.data), output);
        case MessageType::COMPONENT_LIST:   // NEW
            return encode_component_list_response(std::get<ComponentListResponse>(message.data), output);
        default:
            return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
}


ProtocolStatus decode_message(const std::string& input, Message& output) {
    if (input.size() < 8 || input.substr(0, 5) != "DISC:") {
        return ProtocolStatus::INVALID_MESSAGE_TYPE;
    }
    std::string msg_type = input.substr(5, 3);
    if (msg_type == "REG") {
        RegistrationRequest req;
        ProtocolStatus status = decode_registration_request(input, req);
        if (status == ProtocolStatus::OK) output = Message(req);
        return status;
    } else if (msg_type == "ACK") {
        RegistrationResponse res;
        ProtocolStatus status = decode_registration_response(input, res);
        if (status == ProtocolStatus::OK) output = Message(res);
        return status;
    } else if (msg_type == "PNG") {
        KeepalivePing ping;
        ProtocolStatus status = decode_keepalive_ping(input, ping);
        if (status == ProtocolStatus::OK) output = Message(ping);
        return status;
    } else if (msg_type == "PON") {
        KeepaliveResponse pong;
        ProtocolStatus status = decode_keepalive_response(input, pong);
        if (status == ProtocolStatus::OK) output = Message(pong);
        return status;
    } else if (msg_type == "ERR") {
        ErrorMessage err;
        ProtocolStatus status = decode_error_message(input, err);
        if (status == ProtocolStatus::OK) output = Message(err);
        return status;
    } else if (msg_type == "QRY") {  // NEW: Component query
        ComponentQuery query;
        ProtocolStatus status = decode_component_query(input, query);
        if (status == ProtocolStatus::OK) output = Message(query);
        return status;
    } else if (msg_type == "LST") {  // NEW: Component list
        ComponentListResponse list;
        ProtocolStatus status = decode_component_list_response(input, list);
        if (status == ProtocolStatus::OK) output = Message(list);
        return status;
    }
    return ProtocolStatus::INVALID_MESSAGE_TYPE;
}

// --- Individual Message Type Implementations ---

ProtocolStatus encode_registration_request(const RegistrationRequest& request, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:REG;"
       << component_type_to_string(request.componentType) << ";"
       << (request.idRequestType == IdRequestType::AUTOMATIC ? "A" : "S") << ";"
       << static_cast<int>(request.groupId) << ";"
       << static_cast<int>(request.idInGroup) << ";"
       << request.componentName << ";"
       << request.listenAddress << ";"
       << request.listenPort << ";"
       << request.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_registration_request(const std::string& input, RegistrationRequest& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 9) return ProtocolStatus::MALFORMED_MESSAGE;
    try {
        output.componentType = string_to_component_type(parts[1]);
        output.idRequestType = (parts[2] == "A") ? IdRequestType::AUTOMATIC : IdRequestType::STATIC;
        output.groupId = static_cast<uint8_t>(std::stoi(parts[3]));
        output.idInGroup = static_cast<uint8_t>(std::stoi(parts[4]));
        output.componentName = parts[5];
        output.listenAddress = parts[6];
        output.listenPort = parts[7];
        output.humanReadableMessage = parts[8];
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    return ProtocolStatus::OK;
}

ProtocolStatus encode_registration_response(const RegistrationResponse& response, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:ACK;"
       << static_cast<int>(response.responseCode) << ";"
       << static_cast<int>(response.assignedGroupId) << ";"
       << static_cast<int>(response.assignedIdInGroup) << ";"
       << response.connectionTargetType << ";"
       << response.connectionTargetAddress << ";"
       << response.connectionTargetPort << ";"
       << response.connectionTargetId << ";"
       << response.configJson << ";"
       << response.detectedClientAddress << ";"  // NEW FIELD
       << response.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_registration_response(const std::string& input, RegistrationResponse& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 11) return ProtocolStatus::MALFORMED_MESSAGE;
    try {
        output.responseCode = static_cast<ResponseCode>(std::stoi(parts[1]));
        output.assignedGroupId = static_cast<uint8_t>(std::stoi(parts[2]));
        output.assignedIdInGroup = static_cast<uint8_t>(std::stoi(parts[3]));
        output.connectionTargetType = parts[4];
        output.connectionTargetAddress = parts[5];
        output.connectionTargetPort = parts[6];
        output.connectionTargetId = static_cast<uint16_t>(std::stoi(parts[7]));
        output.configJson = parts[8];
        output.detectedClientAddress = parts[9];  // NEW FIELD
        output.humanReadableMessage = parts[10];   // Shifted index
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    return ProtocolStatus::OK;
}

ProtocolStatus encode_keepalive_ping(const KeepalivePing& ping, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:PNG;" << ping.componentId << ";" << ping.status << ";" << ping.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_keepalive_ping(const std::string& input, KeepalivePing& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 4) return ProtocolStatus::MALFORMED_MESSAGE;
    output.componentId = parts[1];
    output.status = parts[2];
    output.humanReadableMessage = parts[3];
    return ProtocolStatus::OK;
}

ProtocolStatus encode_keepalive_response(const KeepaliveResponse& response, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:PON;" << response.response << ";" << response.nextIntervalMs << ";" << response.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_keepalive_response(const std::string& input, KeepaliveResponse& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 4) return ProtocolStatus::MALFORMED_MESSAGE;
    try {
        output.response = parts[1];
        output.nextIntervalMs = std::stoi(parts[2]);
        output.humanReadableMessage = parts[3];
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    return ProtocolStatus::OK;
}

ProtocolStatus encode_error_message(const ErrorMessage& error, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:ERR;" << static_cast<int>(error.errorCode) << ";" << error.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_error_message(const std::string& input, ErrorMessage& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 3) return ProtocolStatus::MALFORMED_MESSAGE;
    try {
        output.errorCode = static_cast<ErrorCode>(std::stoi(parts[1]));
        output.humanReadableMessage = parts[2];
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    return ProtocolStatus::OK;
}

// NEW: Component Query Implementation
ProtocolStatus encode_component_query(const ComponentQuery& query, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:QRY;" << query.componentTypeFilter << ";" << query.humanReadableMessage;
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_component_query(const std::string& input, ComponentQuery& output) {
    auto parts = split_string(input, ';');
    if (parts.size() != 3) return ProtocolStatus::MALFORMED_MESSAGE;
    output.componentTypeFilter = parts[1];
    output.humanReadableMessage = parts[2];
    return ProtocolStatus::OK;
}

ProtocolStatus encode_component_list_response(const ComponentListResponse& response, std::string& output) {
    std::ostringstream ss;
    ss << "DISC:LST;" << response.componentCount;
    
    // Add each component data entry
    for (const auto& componentData : response.componentDataList) {
        ss << ";" << componentData;
    }
    
    // Add human readable message at the end
    ss << ";" << response.humanReadableMessage;
    
    output = ss.str();
    return ProtocolStatus::OK;
}

ProtocolStatus decode_component_list_response(const std::string& input, ComponentListResponse& output) {
    auto parts = split_string(input, ';');
    if (parts.size() < 3) return ProtocolStatus::MALFORMED_MESSAGE;
    
    try {
        output.componentCount = static_cast<uint32_t>(std::stoi(parts[1]));
        
        // Clear the component list
        output.componentDataList.clear();
        
        // Extract component data (between count and human readable message)
        // Format: DISC:LST;count;comp1;comp2;...;compN;humanReadableMessage
        // So component data goes from parts[2] to parts[2 + componentCount - 1]
        if (parts.size() < 2 + output.componentCount + 1) {
            return ProtocolStatus::MALFORMED_MESSAGE;
        }
        
        for (uint32_t i = 0; i < output.componentCount; ++i) {
            if (2 + i < parts.size()) {
                output.componentDataList.push_back(parts[2 + i]);
            }
        }
        
        // Human readable message is the last field
        if (parts.size() > 2 + output.componentCount) {
            output.humanReadableMessage = parts[2 + output.componentCount];
        } else {
            output.humanReadableMessage = "";
        }
        
    } catch (const std::exception&) {
        return ProtocolStatus::INVALID_FORMAT;
    }
    return ProtocolStatus::OK;
}

// NEW: Component data parsing utilities
ComponentData parse_component_data(const std::string& data_str) {
    ComponentData data;
    auto parts = split_string(data_str, ':');
    
    if (parts.size() >= 3) {
        data.component_type = parts[0];
        try {
            data.group_id = static_cast<uint8_t>(std::stoi(parts[1]));
            data.id_in_group = static_cast<uint8_t>(std::stoi(parts[2]));
            
            // Subtype is optional, default to 0 for backward compatibility
            if (parts.size() >= 4) {
                data.subtype = static_cast<uint8_t>(std::stoi(parts[3]));
            } else {
                data.subtype = 0;
            }
        } catch (const std::exception&) {
            // If parsing fails, use default values
            data.group_id = 0;
            data.id_in_group = 0;
            data.subtype = 0;
        }
    }
    
    return data;
}

std::string format_component_data(const ComponentData& data) {
    std::ostringstream ss;
    ss << data.component_type << ":" 
       << static_cast<int>(data.group_id) << ":" 
       << static_cast<int>(data.id_in_group) << ":" 
       << static_cast<int>(data.subtype);
    return ss.str();
}

} // namespace discovery_protocol