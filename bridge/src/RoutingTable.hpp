#ifndef ROUTING_TABLE_HPP
#define ROUTING_TABLE_HPP

#include "common_types.hpp" // For new RoutingKey, MPSCQueueType, Message
#include "logging/logger.h" // For CppLogging

#include <unordered_map>
#include <vector>
#include <memory>       // For std::shared_ptr
#include <shared_mutex> // For std::shared_mutex

class RoutingTable {
public:
    explicit RoutingTable(size_t max_expected_routes);
    std::vector<std::shared_ptr<MPSCQueueType>> get_destinations(const RoutingKey& key) const;
    void add_route(const RoutingKey& key, std::shared_ptr<MPSCQueueType> destination_queue);
    void remove_routes_for_key(const RoutingKey& key);

private:
    // Map: (source_id, message_type) → list of destination queues
    std::unordered_map<RoutingKey, std::vector<std::shared_ptr<MPSCQueueType>>> actual_map_;
    // Read-write lock for thread-safe concurrent access
    mutable std::shared_mutex map_mutex_;
    // Capacity reservation hint (default 1000 in BridgeControlPlane)
    size_t max_expected_routes_config_;
    // Logger instance
    CppLogging::Logger logger_;
};

#endif // ROUTING_TABLE_HPP