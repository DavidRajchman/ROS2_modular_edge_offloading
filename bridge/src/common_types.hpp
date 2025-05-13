#ifndef COMMON_TYPES_HPP
#define COMMON_TYPES_HPP

#include <string>
#include <vector>
#include <memory> // For std::shared_ptr
#include <functional> // For std::hash

// Assuming moodycamel's MPSC queue.
// You'll need to ensure this header is available in your include paths.
// If you don't have it yet, you can temporarily use a placeholder,
// but for actual MPSC functionality, a proper queue is needed.
#include "concurrentqueue.h"

// A simple placeholder for a message
struct Message {
    // For now, let's assume the message data itself is a byte vector.
    // The actual structure of ROS2 messages might be more complex and handled
    // by ROS2 libraries, but for the Bridge's internal forwarding,
    // it might treat the payload as opaque bytes after parsing the header.
    std::vector<char> data;

    // You might add other fields if the Bridge needs to inspect/modify them,
    // but for simple forwarding, the payload might be opaque.
};

// Define the MPSC Queue type we'll be using
// This queue will hold shared_ptr to Message objects.
using MPSCQueueType = moodycamel::ConcurrentQueue<std::shared_ptr<Message>>;

// Define the Routing Key
struct RoutingKey {
    std::string source_id; // e.g., VHC_ID or MEC_ID
    std::string topic;     // e.g., ROS2 topic name

    // Equality operator for std::unordered_map
    bool operator==(const RoutingKey& other) const {
        return source_id == other.source_id && topic == other.topic;
    }
};

// Hash function for RoutingKey for std::unordered_map
// This needs to be in the std namespace or provided as a custom hasher to std::unordered_map
namespace std {
    template <>
    struct hash<RoutingKey> {
        std::size_t operator()(const RoutingKey& k) const {
            // A simple hash combination.
            // You might want a more robust hash function for production.
            std::size_t h1 = std::hash<std::string>()(k.source_id);
            std::size_t h2 = std::hash<std::string>()(k.topic);
            return h1 ^ (h2 << 1); // Combine hashes
        }
    };
} // namespace std

// Placeholder function to parse message header and extract RoutingKey
// This function will be implemented properly once the header format is defined.
// It takes the raw message data (or a part of it representing the header).
inline RoutingKey parse_message_header(const Message& received_message) {
    // TODO: Implement actual header parsing logic here once header format is defined.
    // The header is expected to be a byte array at the start of received_message.data.
    // This function will need to:
    // 1. Access the beginning of received_message.data.
    // 2. Deserialize the Source_Identifier and Topic from these bytes according to the defined format.
    // 3. Handle potential errors (e.g., insufficient data for header, malformed header).

    // For now, returning a dummy/placeholder key based on simple checks for varied testing.
    // This is NOT how actual header parsing would work.
    if (received_message.data.empty()) {
        return {"UNKNOWN_SOURCE_H", "UNKNOWN_TOPIC_H"}; // "H" for Header-parsed
    }
    
    // Example: very simple placeholder logic based on message content
    if (received_message.data.size() > 4 && received_message.data[0] == 'V' && received_message.data[1] == '1') {
        return {"VHC1_FROM_HEADER", "GPS_FROM_HEADER"};
    } else if (received_message.data.size() > 4 && received_message.data[0] == 'M' && received_message.data[1] == '1') {
        return {"MEC1_FROM_HEADER", "RESULT_FROM_HEADER"};
    }

    return {"DEFAULT_SOURCE_H", "DEFAULT_TOPIC_H"};
}

#endif // COMMON_TYPES_HPP