#include "RoutingTable.hpp"
#include "logging/logger.h" // For CppLogging
#include <algorithm> // For std::remove

RoutingTable::RoutingTable(size_t max_expected_routes)
    : max_expected_routes_config_(max_expected_routes), logger_("bridge") {
    actual_map_.reserve(max_expected_routes_config_);
    logger_.Info("RoutingTable: Initialized and reserved space for {} potential key entries.", max_expected_routes_config_);
}

std::vector<std::shared_ptr<MPSCQueueType>> RoutingTable::get_destinations(const RoutingKey& key) const {
    std::shared_lock<std::shared_mutex> lock(map_mutex_); // Acquire shared lock for reading
    auto it = actual_map_.find(key);
    if (it != actual_map_.end()) {
        return it->second; // Return a copy of the vector of queue shared_ptrs
    }
    return {}; // Return an empty vector if no route found
}

void RoutingTable::add_route(const RoutingKey& key, std::shared_ptr<MPSCQueueType> destination_queue) {
    if (!destination_queue) {
        logger_.Warn("RoutingTable: Attempted to add a null destination queue for SourceID: {}, MsgType: {}. Ignoring.",
                 key.source_id, key.message_type);
        return;
    }
    std::unique_lock<std::shared_mutex> lock(map_mutex_); // Acquire exclusive lock for writing
    actual_map_[key].push_back(destination_queue);
    logger_.Info("RoutingTable: Added route for SourceID: {} (Group: {}, IDInGroup: {}), MsgType: {}.", 
             key.source_id, (key.source_id >> 8), (key.source_id & 0xFF), key.message_type);
}

void RoutingTable::remove_routes_for_key(const RoutingKey& key) {
    std::unique_lock<std::shared_mutex> lock(map_mutex_); // Acquire exclusive lock for writing
    auto it = actual_map_.find(key);
    if (it != actual_map_.end()) {
        actual_map_.erase(it);
        logger_.Info("RoutingTable: Removed all routes for SourceID: {} (Group: {}, IDInGroup: {}), MsgType: {}.", 
                 key.source_id, (key.source_id >> 8), (key.source_id & 0xFF), key.message_type);
    } else {
        logger_.Warn("RoutingTable: No routes found to remove for SourceID: {}, MsgType: {}.",
                 key.source_id, key.message_type);
    }
}

