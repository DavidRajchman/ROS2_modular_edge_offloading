#ifndef ROUTING_TABLE_HPP
#define ROUTING_TABLE_HPP

#include "common_types.hpp" // For RoutingKey, MPSCQueueType, Message

#include <unordered_map>
#include <vector>
#include <string>
#include <memory>       // For std::shared_ptr
#include <shared_mutex> // For std::shared_mutex
#include <vector>

class RoutingTable {
public:
    // Constructor, takes the maximum expected routes to pre-reserve capacity.
    explicit RoutingTable(size_t max_expected_routes);

    // Method for Data Plane to get destination queues for a given key.
    // Returns a vector of shared_ptrs to MPSC queues.
    // The vector might be empty if no route is found.
    // This method is const and thread-safe for concurrent reads.
    std::vector<std::shared_ptr<MPSCQueueType>> get_destinations(const RoutingKey& key) const;

    // --- Methods for Control Plane (to be fully developed later with proper CP logic) ---

    // Example: A simple method to add a route for testing or initial setup.
    // In the final version, this will be called by the Control Plane and
    // will acquire an exclusive lock.
    void add_route_for_testing(const RoutingKey& key, std::shared_ptr<MPSCQueueType> destination_queue);

    // Example: A simple method to remove all routes for a specific key.
    void remove_all_routes_for_key_for_testing(const RoutingKey& key);

    // Add more Control Plane methods as needed, e.g.:
    // void remove_specific_route(const RoutingKey& key, std::shared_ptr<MPSCQueueType> destination_to_remove);
    // void clear_all_routes_for_destination_queue(std::shared_ptr<MPSCQueueType> queue_to_clear);

private:
    // The actual hash map storing routing rules.
    std::unordered_map<RoutingKey, std::vector<std::shared_ptr<MPSCQueueType>>> actual_map_;
    
    // Read-write mutex to protect concurrent access to the map.
    // `mutable` allows locking in const methods like `get_destinations`.
    mutable std::shared_mutex map_mutex_; 
                                         
    // Configuration for initial reservation.
    size_t max_expected_routes_config_;
};

#endif // ROUTING_TABLE_HPP