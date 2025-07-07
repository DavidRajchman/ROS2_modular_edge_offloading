#ifndef CONTROL_PLANE_PROTOCOL_HPP
#define CONTROL_PLANE_PROTOCOL_HPP

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace gateway::protocol {

// Use nlohmann::json for convenience
using json = nlohmann::json;

/**
 * @brief Defines the types of messages in the Control Plane Protocol.
 */
enum class ControlPlaneMessageType {
    UNKNOWN,
    REQUEST_OFFLOADING,
    REQUEST_RESPONSE,
    SESSION_KEEPALIVE,
    SESSION_TEARDOWN
};

/**
 * @brief Represents a request from the Gateway to the Bridge to offload a task.
 */
struct OffloadingRequest {
    std::string request_id;
    std::string component_id; // e.g., "10:1"
    std::string task_id;
};

/**
 * @brief Represents the Bridge's response to an offloading request.
 */
struct OffloadingResponse {
    std::string request_id;
    bool approved;
    std::string message;
    std::string data_plane_host; // The host for the data connection
    int data_plane_port;         // The port for the data connection
};

/**
 * @brief Represents a keepalive message to maintain an active session.
 */
struct SessionKeepalive {
    std::string request_id;
    std::string component_id;
};

// --- Serialization Functions ---

/**
 * @brief Serializes an OffloadingRequest to a JSON string.
 */
inline std::string serialize(const OffloadingRequest& req) {
    json j = {
        {"type", "REQUEST_OFFLOADING"},
        {"request_id", req.request_id},
        {"component_id", req.component_id},
        {"task_id", req.task_id}
    };
    return j.dump();
}

/**
 * @brief Serializes a SessionKeepalive to a JSON string.
 */
inline std::string serialize(const SessionKeepalive& req) {
    json j = {
        {"type", "SESSION_KEEPALIVE"},
        {"request_id", req.request_id},
        {"component_id", req.component_id}
    };
    return j.dump();
}


// --- Deserialization Functions ---

/**
 * @brief Deserializes a JSON string into an OffloadingResponse.
 * @throws std::invalid_argument if parsing or validation fails.
 */
inline OffloadingResponse deserialize_response(const std::string& json_str) {
    OffloadingResponse resp;
    try {
        json j = json::parse(json_str);
        if (j.at("type") != "REQUEST_RESPONSE") {
            throw std::invalid_argument("Invalid message type for response.");
        }
        j.at("request_id").get_to(resp.request_id);
        j.at("approved").get_to(resp.approved);
        j.at("message").get_to(resp.message);
        if (resp.approved) {
            j.at("data_plane_host").get_to(resp.data_plane_host);
            j.at("data_plane_port").get_to(resp.data_plane_port);
        }
    } catch (const json::exception& e) {
        throw std::invalid_argument("Failed to parse OffloadingResponse: " + std::string(e.what()));
    }
    return resp;
}

/**
 * @brief Gets the message type from a raw JSON string.
 */
inline ControlPlaneMessageType get_message_type(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        std::string type_str = j.value("type", "UNKNOWN");
        if (type_str == "REQUEST_RESPONSE") return ControlPlaneMessageType::REQUEST_RESPONSE;
        if (type_str == "SESSION_TEARDOWN") return ControlPlaneMessageType::SESSION_TEARDOWN;
    } catch (const json::exception&) {
        return ControlPlaneMessageType::UNKNOWN;
    }
    return ControlPlaneMessageType::UNKNOWN;
}

} // namespace gateway::protocol

#endif // CONTROL_PLANE_PROTOCOL_HPP