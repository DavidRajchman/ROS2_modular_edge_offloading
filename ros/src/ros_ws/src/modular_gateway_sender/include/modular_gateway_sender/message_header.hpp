/**
 * @file message_header.hpp
 * @brief Defines the message protocol and header structure for the modular gateway system.
 * 
 * This file contains the definitions for message types, header structure, and flag management
 * used in the communication protocol between ROS nodes and network transports. It provides
 * utilities for creating properly formatted binary headers with customizable options.
 */

#ifndef MESSAGE_HEADER_HPP
#define MESSAGE_HEADER_HPP

#include <cstdint>  // For fixed-size integer types
#include <string>   // For std::string
#include <vector>   // For std::vector

namespace gateway {

/**
 * @brief Enumeration of supported message types in the gateway protocol.
 * 
 * Each message type corresponds to a specific data format that can be
 * transmitted through the gateway. New types can be added as needed.
 */
enum class MessageType : uint8_t {
  //basic messages - range [1-10]
  STRING = 1,       ///< Text/string data
  INT32 = 2,        ///< 32-bit signed integer
  FLOAT32 = 3,      ///< 32-bit floating point number
  BOOL = 4,         ///< Boolean value

  //sensor_msgs - range [11 - 100]
  smLASERSCAN = 11,  ///< sensor_msgs/LaserScan Message
  
};

/// Magic number used to identify the start of a valid message header
constexpr uint16_t HEADER_MAGIC = 0xA5C3;

/**
 * @brief Bit flag definitions for message header options.
 * 
 * These flags are used in the header to indicate various properties
 * of the message being transmitted.
 */
constexpr uint8_t FLAG_HAS_TIMESTAMP = 0x80;  ///< Bit 7: Message includes timestamp information
constexpr uint8_t FLAG_FRAGMENTED = 0x40;     ///< Bit 6: Message is part of a fragmented sequence
constexpr uint8_t FLAG_SERIALIZED = 0x20;     ///< Bit 5: Message contains serialized data
constexpr uint8_t FLAG_HAS_CHECKSUM = 0x10;   ///< Bit 4: Message includes checksum for validation
constexpr uint8_t FLAG_COMPRESSED = 0x08;     ///< Bit 3: Message data is compressed
constexpr uint8_t FLAG_VERSION_MASK = 0x07;   ///< Bits 0-2: Protocol version (0-7)

/**
 * @brief Structure for easier management of message header options.
 * 
 * This struct provides a more user-friendly way to set message flags
 * compared to manually manipulating bit flags. It offers conversion
 * between the struct representation and the raw flags byte.
 */
struct MessageOptions {
  bool has_timestamp = false;  ///< Whether to include timestamp information
  bool fragmented = false;     ///< Whether this is part of a fragmented message
  bool serialized = false;     ///< Whether the message contains serialized data
  bool has_checksum = false;   ///< Whether to include a checksum
  bool compressed = false;     ///< Whether the message data is compressed
  uint8_t version = 0;         ///< Protocol version (0-7)
  
  /**
   * @brief Converts the options to a raw flags byte.
   * 
   * Combines all the boolean flags and version into a single byte
   * that can be used in the message header.
   * 
   * @return uint8_t The combined flags byte
   */
  uint8_t to_flags() const {
    uint8_t flags = 0;
    if (has_timestamp) flags |= FLAG_HAS_TIMESTAMP;
    if (fragmented) flags |= FLAG_FRAGMENTED;
    if (serialized) flags |= FLAG_SERIALIZED;
    if (has_checksum) flags |= FLAG_HAS_CHECKSUM;
    if (compressed) flags |= FLAG_COMPRESSED;
    flags |= (version & FLAG_VERSION_MASK);
    return flags;
  }
  
  /**
   * @brief Constructs MessageOptions from a raw flags byte.
   * 
   * Extracts individual flags and version from a combined flags byte.
   * 
   * @param flags The raw flags byte
   */
  explicit MessageOptions(uint8_t flags) {
    has_timestamp = (flags & FLAG_HAS_TIMESTAMP) != 0;
    fragmented = (flags & FLAG_FRAGMENTED) != 0;
    serialized = (flags & FLAG_SERIALIZED) != 0;
    has_checksum = (flags & FLAG_HAS_CHECKSUM) != 0;
    compressed = (flags & FLAG_COMPRESSED) != 0;
    version = flags & FLAG_VERSION_MASK;
  }
  
  /// Default constructor initializes all options to false/zero
  MessageOptions() = default;
};

/**
 * @brief Creates a binary message header with the specified parameters.
 * 
 * This function creates a binary header with the following format:
 * - Magic bytes (2 bytes): 0xA5C3 to identify valid messages
 * - Flags byte (1 byte): Contains feature flags and protocol version
 * - Message type (1 byte): Identifier from the MessageType enum
 * - Payload size (4 bytes): Size of the message data in bytes (big endian)
 * - Topic length (1 byte): Length of the topic name string
 * - Topic name (variable): UTF-8 encoded topic name
 * 
 * @deprecated Consider using the MessageOptions version for better readability
 * 
 * @param topic The topic name for the message
 * @param type The message type from MessageType enum
 * @param data_size Size of the message payload in bytes
 * @param flags Raw flags byte (default: 0)
 * @return std::vector<uint8_t> Binary header as a byte vector
 */
inline std::vector<uint8_t> create_header(
  const std::string& topic,
  MessageType type,
  uint32_t data_size,
  uint8_t flags = 0)
{
  std::vector<uint8_t> header;
  
  // Reserve space for efficiency
  header.reserve(9 + topic.size());
  
  // Magic bytes
  header.push_back(HEADER_MAGIC >> 8);    // High byte
  header.push_back(HEADER_MAGIC & 0xFF);  // Low byte
  
  // Flags byte (version 0 by default)
  header.push_back(flags & 0xFF);
  
  // Message type
  header.push_back(static_cast<uint8_t>(type));
  
  // Payload size (4 bytes, big endian)
  header.push_back((data_size >> 24) & 0xFF);  // Most significant byte
  header.push_back((data_size >> 16) & 0xFF);  
  header.push_back((data_size >> 8) & 0xFF);   
  header.push_back(data_size & 0xFF);          // Least significant byte
  
  // Topic length (1 byte)
  uint8_t topic_len = std::min(topic.size(), static_cast<size_t>(255));
  header.push_back(topic_len);
  
  // Topic name
  header.insert(header.end(), topic.begin(), topic.begin() + topic_len);
  
  return header;
}

/**
 * @brief Creates a binary message header using MessageOptions.
 * 
 * This overload allows for using the more structured MessageOptions
 * approach to set header flags instead of raw bit manipulation.
 * 
 * @param topic The topic name for the message
 * @param type The message type from MessageType enum
 * @param data_size Size of the message payload in bytes
 * @param options MessageOptions struct containing flag settings
 * @return std::vector<uint8_t> Binary header as a byte vector
 */
inline std::vector<uint8_t> create_header(
  const std::string& topic,
  MessageType type,
  uint32_t data_size,
  const MessageOptions& options)
{
  // Convert the options to a flags byte and use the raw flags version
  return create_header(topic, type, data_size, options.to_flags());
}

} // namespace gateway

#endif // MESSAGE_HEADER_HPP