#ifndef MESSAGE_HEADER_HPP
#define MESSAGE_HEADER_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace gateway {

// Message types enum
enum class MessageType : uint8_t {
  STRING = 1,
  INT32 = 2,
  FLOAT32 = 3,
  BOOL = 4,
  IMAGE = 5,
  // Add more as needed
};

// Header constants
constexpr uint16_t HEADER_MAGIC = 0xA5C3;

// Flags bit positions
constexpr uint8_t FLAG_HAS_TIMESTAMP = 0x80;  // Bit 7
constexpr uint8_t FLAG_FRAGMENTED = 0x40;     // Bit 6
constexpr uint8_t FLAG_SERIALIZED = 0x20;     // Bit 5
constexpr uint8_t FLAG_HAS_CHECKSUM = 0x10;   // Bit 4
constexpr uint8_t FLAG_COMPRESSED = 0x08;     // Bit 3
constexpr uint8_t FLAG_VERSION_MASK = 0x07;   // Bits 0-2

// Create binary header
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
  header.push_back((data_size >> 24) & 0xFF);
  header.push_back((data_size >> 16) & 0xFF);
  header.push_back((data_size >> 8) & 0xFF);
  header.push_back(data_size & 0xFF);
  
  // Topic length (1 byte)
  uint8_t topic_len = std::min(topic.size(), static_cast<size_t>(255));
  header.push_back(topic_len);
  
  // Topic name
  header.insert(header.end(), topic.begin(), topic.begin() + topic_len);
  
  return header;
}

} // namespace gateway

#endif // MESSAGE_HEADER_HPP