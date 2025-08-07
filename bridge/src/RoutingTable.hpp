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
    // Constructor, takes the maximum expected routes to pre-reserve capacity.
    explicit RoutingTable(size_t max_expected_routes);

    // Method for Data Plane to get destination queues for a given key.
    // Returns a vector of shared_ptrs to MPSC queues.
    // The vector might be empty if no route is found.
    // This method is const and thread-safe for concurrent reads.
    std::vector<std::shared_ptr<MPSCQueueType>> get_destinations(const RoutingKey& key) const;

    // --- Methods for Control Plane ---

    // Adds a route from the given key to the specified destination queue.
    // If the key already exists, the queue is added to the list of destinations.
    void add_route(const RoutingKey& key, std::shared_ptr<MPSCQueueType> destination_queue);

    // Removes all routes associated with the given key.
    void remove_routes_for_key(const RoutingKey& key);

    
private:
    std::unordered_map<RoutingKey, std::vector<std::shared_ptr<MPSCQueueType>>> actual_map_;
    mutable std::shared_mutex map_mutex_; 
    size_t max_expected_routes_config_;
    CppLogging::Logger logger_;
};

#endif // ROUTING_TABLE_HPP