#ifndef COMMON_TYPES_HPP
#define COMMON_TYPES_HPP

#include <transport/logging_utils.hpp> // For logging


#include <string>
#include <vector>
#include <memory>     // For std::shared_ptr
#include <functional> // For std::hash
#include <cstdint>    // For uint8_t, uint16_t
#include <optional>   // For std::optional
#include <cstring>    // For memcpy
#include <arpa/inet.h> // For ntohs, ntohl

#include "concurrentqueue.h"

// --- Modular GW Header Constants ---
namespace ModGW {
namespace Header {
    constexpr size_t MAGIC_NUMBER_OFFSET = 0;
    constexpr size_t FLAGS_OFFSET = 2;
    constexpr size_t MESSAGE_TYPE_OFFSET = 3;
    constexpr size_t ID_GROUP_OFFSET = 4;
    constexpr size_t IDENTIFIER_IN_GROUP_OFFSET = 5;
    constexpr size_t PAYLOAD_SIZE_FIELD_OFFSET = 6;
    constexpr size_t TOPIC_LENGTH_FIELD_OFFSET = 10;
    constexpr size_t TOPIC_NAME_START_OFFSET = 11;

    constexpr size_t MAGIC_NUMBER_LEN = 2;
    constexpr size_t PAYLOAD_SIZE_FIELD_LEN = 4;
    constexpr size_t TOPIC_LENGTH_FIELD_LEN = 1;

    constexpr size_t MIN_HEADER_LEN_BEFORE_TOPIC_NAME = TOPIC_NAME_START_OFFSET;

    constexpr uint16_t EXPECTED_MAGIC_NUMBER = 0xA5C3;
} // namespace Header
} // namespace ModGW

inline uint16_t read_uint16_big_endian(const unsigned char* buffer) {
    uint16_t value;
    std::memcpy(&value, buffer, sizeof(uint16_t));
    return ntohs(value);
}

inline uint32_t read_uint32_big_endian(const unsigned char* buffer) {
    uint32_t value;
    std::memcpy(&value, buffer, sizeof(uint32_t));
    return ntohl(value);
}

struct Message {
    std::vector<unsigned char> data;
};

using MPSCQueueType = moodycamel::ConcurrentQueue<std::shared_ptr<Message>>;

struct RoutingKey {
    uint16_t source_id;
    uint8_t message_type;

    bool operator==(const RoutingKey& other) const {
        return source_id == other.source_id && message_type == other.message_type;
    }
};

inline RoutingKey construct_routing_key(uint8_t id_group, uint8_t id_in_group, uint8_t msg_type) {
    uint16_t combined_source_id = (static_cast<uint16_t>(id_group) << 8) | static_cast<uint16_t>(id_in_group);
    return {combined_source_id, msg_type};
}

namespace std {
    template <>
    struct hash<RoutingKey> {
        std::size_t operator()(const RoutingKey& k) const {
            std::size_t h1 = std::hash<uint16_t>()(k.source_id);
            std::size_t h2 = std::hash<uint8_t>()(k.message_type);
            return h1 ^ (h2 << 1);
        }
    };
}

struct ParsedHeaderInfo {
    RoutingKey routing_key;
    size_t total_message_length;
    size_t payload_offset;
    size_t payload_size;
};

inline std::optional<ParsedHeaderInfo> parse_message_header(const unsigned char* buffer, size_t buffer_size) {
    if (buffer_size < ModGW::Header::MIN_HEADER_LEN_BEFORE_TOPIC_NAME) {
        LOG_ERROR("parse_message_header: Insufficient data for header parsing.");
        return std::nullopt;
    }

    uint16_t magic_number = read_uint16_big_endian(buffer + ModGW::Header::MAGIC_NUMBER_OFFSET);
    if (magic_number != ModGW::Header::EXPECTED_MAGIC_NUMBER) {
        LOG_ERROR("parse_message_header: Invalid magic number.");
        return std::nullopt;
    }

    uint8_t msg_type = buffer[ModGW::Header::MESSAGE_TYPE_OFFSET];
    uint8_t id_group = buffer[ModGW::Header::ID_GROUP_OFFSET];
    uint8_t id_in_group = buffer[ModGW::Header::IDENTIFIER_IN_GROUP_OFFSET];
    RoutingKey key = construct_routing_key(id_group, id_in_group, msg_type);

    uint8_t topic_length = buffer[ModGW::Header::TOPIC_LENGTH_FIELD_OFFSET];
    uint32_t payload_size = read_uint32_big_endian(buffer + ModGW::Header::PAYLOAD_SIZE_FIELD_OFFSET);

    size_t total_header_length = ModGW::Header::MIN_HEADER_LEN_BEFORE_TOPIC_NAME + topic_length;
    size_t total_message_length = total_header_length + payload_size;

    return ParsedHeaderInfo{key, total_message_length, total_header_length, payload_size};
}

inline std::optional<ParsedHeaderInfo> parse_message_header(const std::vector<unsigned char>& data_buffer) {
    if (data_buffer.empty()) {
        return std::nullopt;
    }
    return parse_message_header(data_buffer.data(), data_buffer.size());
}

#endif // COMMON_TYPES_HPP