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

/**
 * @brief Reads a uint16_t from a buffer in big-endian format and converts it to host byte order.
 * @param buffer Pointer to the buffer containing the big-endian uint16_t.
 * @return The uint16_t value in host byte order.
 */
inline uint16_t read_uint16_big_endian(const unsigned char* buffer) {
    uint16_t value;
    std::memcpy(&value, buffer, sizeof(uint16_t));
    return ntohs(value);
}

/**
 * @brief Reads a uint32_t from a buffer in big-endian format and converts it to host byte order.
 * @param buffer Pointer to the buffer containing the big-endian uint32_t.
 * @return The uint32_t value in host byte order.
 */
inline uint32_t read_uint32_big_endian(const unsigned char* buffer) {
    uint32_t value;
    std::memcpy(&value, buffer, sizeof(uint32_t));
    return ntohl(value);
}

/**
 * @brief Represents a generic message, containing raw byte data.
 */
struct Message {
    std::vector<unsigned char> data;
};

/**
 * @brief Type alias for a Multiple Producer Single Consumer (MPSC) queue holding shared pointers to Message objects.
 */
using MPSCQueueType = moodycamel::ConcurrentQueue<std::shared_ptr<Message>>;

/**
 * @brief Defines a key used for routing messages, based on source and message type.
 */
struct RoutingKey {
    uint16_t source_id;    ///< Combined ID_GROUP and IDENTIFIER_IN_GROUP.
    uint8_t message_type;  ///< The specific type/topic of the message.

    /**
     * @brief Compares two RoutingKey objects for equality.
     * @param other The other RoutingKey to compare against.
     * @return True if both source_id and message_type are equal, false otherwise.
     */
    bool operator==(const RoutingKey& other) const {
        return source_id == other.source_id && message_type == other.message_type;
    }
};

/**
 * @brief Constructs a RoutingKey from individual ID group, ID in group, and message type components.
 * @param id_group The identifier for the group of the sender.
 * @param id_in_group The identifier for the specific instance within the group.
 * @param msg_type The message type identifier.
 * @return A RoutingKey object.
 */
inline RoutingKey construct_routing_key(uint8_t id_group, uint8_t id_in_group, uint8_t msg_type) {
    uint16_t combined_source_id = (static_cast<uint16_t>(id_group) << 8) | static_cast<uint16_t>(id_in_group);
    return {combined_source_id, msg_type};
}

namespace std {
    /**
     * @brief Specialization of std::hash for RoutingKey, enabling its use in hash-based containers.
     */
    template <>
    struct hash<RoutingKey> {
        /**
         * @brief Calculates the hash value for a RoutingKey.
         * @param k The RoutingKey to hash.
         * @return The calculated hash value.
         */
        std::size_t operator()(const RoutingKey& k) const {
            std::size_t h1 = std::hash<uint16_t>()(k.source_id);
            std::size_t h2 = std::hash<uint8_t>()(k.message_type);
            return h1 ^ (h2 << 1); // Combine hashes
        }
    };
}

/**
 * @brief Holds information parsed from a Modular GW message header.
 */
struct ParsedHeaderInfo {
    RoutingKey routing_key;         ///< The routing key extracted from the header.
    size_t total_message_length;    ///< The total length of the message (header + payload).
    size_t payload_offset;          ///< The offset from the start of the buffer where the payload begins (i.e., total header length).
    size_t payload_size;            ///< The size of the message payload in bytes.
};

/**
 * @brief Parses the Modular GW header from a raw byte buffer.
 * @param buffer Pointer to the start of the message buffer.
 * @param buffer_size The total size of the data available in the buffer.
 * @return An std::optional containing ParsedHeaderInfo if parsing is successful, otherwise std::nullopt.
 */
inline std::optional<ParsedHeaderInfo> parse_message_header(const unsigned char* buffer, size_t buffer_size) {
    if (buffer_size < ModGW::Header::MIN_HEADER_LEN_BEFORE_TOPIC_NAME) {
        LOG_ERROR("parse_message_header: Insufficient data for header parsing (need at least %zu, got %zu).", ModGW::Header::MIN_HEADER_LEN_BEFORE_TOPIC_NAME, buffer_size);
        return std::nullopt;
    }

    uint16_t magic_number = read_uint16_big_endian(buffer + ModGW::Header::MAGIC_NUMBER_OFFSET);
    if (magic_number != ModGW::Header::EXPECTED_MAGIC_NUMBER) {
        LOG_ERROR("parse_message_header: Invalid magic number. Expected 0x%X, got 0x%X.", ModGW::Header::EXPECTED_MAGIC_NUMBER, magic_number);
        return std::nullopt;
    }

    uint8_t msg_type = buffer[ModGW::Header::MESSAGE_TYPE_OFFSET];
    uint8_t id_group = buffer[ModGW::Header::ID_GROUP_OFFSET];
    uint8_t id_in_group = buffer[ModGW::Header::IDENTIFIER_IN_GROUP_OFFSET];
    RoutingKey key = construct_routing_key(id_group, id_in_group, msg_type);

    uint8_t topic_length = buffer[ModGW::Header::TOPIC_LENGTH_FIELD_OFFSET];
    if (buffer_size < ModGW::Header::MIN_HEADER_LEN_BEFORE_TOPIC_NAME + topic_length) {
        LOG_ERROR("parse_message_header: Insufficient data for full topic name (topic_length: %u, buffer_size: %zu, needed: %zu).", topic_length, buffer_size, ModGW::Header::MIN_HEADER_LEN_BEFORE_TOPIC_NAME + topic_length);
        return std::nullopt;
    }
    uint32_t payload_size = read_uint32_big_endian(buffer + ModGW::Header::PAYLOAD_SIZE_FIELD_OFFSET);

    size_t total_header_length = ModGW::Header::MIN_HEADER_LEN_BEFORE_TOPIC_NAME + topic_length;
    size_t total_message_length = total_header_length + payload_size;

    // Basic check: does the buffer even claim to hold the full message?
    // This doesn't guarantee the payload is all there yet, but that the header's claims are plausible for the given buffer_size.
    // The actual check for full message data happens before attempting to extract payload in TransportHandler.
    // if (buffer_size < total_message_length) {
    //     LOG_DEBUG("parse_message_header: Buffer (size %zu) is smaller than indicated total message length (%zu). Header parsed, but full message might not be present yet.", buffer_size, total_message_length);
    // }


    return ParsedHeaderInfo{key, total_message_length, total_header_length, payload_size};
}

/**
 * @brief Parses the Modular GW header from a std::vector of unsigned char.
 * @param data_buffer The vector containing the message data.
 * @return An std::optional containing ParsedHeaderInfo if parsing is successful, otherwise std::nullopt.
 */
inline std::optional<ParsedHeaderInfo> parse_message_header(const std::vector<unsigned char>& data_buffer) {
    if (data_buffer.empty()) {
        LOG_DEBUG("parse_message_header: Data buffer is empty.");
        return std::nullopt;
    }
    return parse_message_header(data_buffer.data(), data_buffer.size());
}

#endif // COMMON_TYPES_HPP