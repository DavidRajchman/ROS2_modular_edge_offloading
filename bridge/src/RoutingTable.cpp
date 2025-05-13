#include "RoutingTable.hpp"
#include <transport/logging_utils.hpp> // Assuming your logging utils

RoutingTable::RoutingTable(size_t max_expected_routes)
    : max_expected_routes_config_(max_expected_routes) {
    actual_map_.reserve(max_expected_routes_config_);
    LOG_INFO("RoutingTable: Initialized and reserved space for %zu routes.", max_expected_routes_config_);
}

std::vector<std::shared_ptr<MPSCQueueType>> RoutingTable::get_destinations(const RoutingKey& key) const {
    std::shared_lock<std::shared_mutex> lock(map_mutex_); // Acquire shared lock for reading
    auto it = actual_map_.find(key);
    if (it != actual_map_.end()) {
        return it->second; // Return a copy of the vector of queue shared_ptrs
    }
    return {}; // Return an empty vector if no route found
}

void RoutingTable::add_route_for_testing(const RoutingKey& key, std::shared_ptr<MPSCQueueType> destination_queue) {
    std::unique_lock<std::shared_mutex> lock(map_mutex_); // Acquire exclusive lock for writing
    actual_map_[key].push_back(destination_queue);
    // Log with more detail if possible, e.g., queue address or an ID if queues have one
    LOG_INFO("RoutingTable (Test): Added route for Source: '%s', Topic: '%s'.", 
             key.source_id.c_str(), key.topic.c_str());
}

void RoutingTable::remove_all_routes_for_key_for_testing(const RoutingKey& key) {
    std::unique_lock<std::shared_mutex> lock(map_mutex_); // Acquire exclusive lock for writing
    auto it = actual_map_.find(key);
    if (it != actual_map_.end()) {
        actual_map_.erase(it);
        LOG_INFO("RoutingTable (Test): Removed all routes for Source: '%s', Topic: '%s'.", 
                 key.source_id.c_str(), key.topic.c_str());
    } else {
        LOG_WARN("RoutingTable (Test): No routes found to remove for Source: '%s', Topic: '%s'.", 
                 key.source_id.c_str(), key.topic.c_str());
    }
}