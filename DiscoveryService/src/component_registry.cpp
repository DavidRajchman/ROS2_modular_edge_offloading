#include "component_registry.hpp"
#include <algorithm>
#include <transport/logging_utils.hpp>

bool ComponentRegistry::register_component(const ComponentInfo& info) {
    if (is_id_taken(info.group_id, info.id_in_group)) {
        return false; // ID conflict
    }

    // Add to primary registry
    registry_by_client_id_[info.client_id] = info;

    // Add to helper maps for lookups
    auto component_id_pair = std::make_pair(info.group_id, info.id_in_group);
    client_id_by_component_id_[component_id_pair] = info.client_id;

    if (info.component_type == discovery_protocol::ComponentType::BRIDGE) {
        bridge_client_ids_.push_back(info.client_id);
    } else if (info.component_type == discovery_protocol::ComponentType::OFFLOAD_MANAGER) {
        om_client_ids_.push_back(info.client_id);
    }

    return true;
}

void ComponentRegistry::unregister_component(uint32_t client_id) {
    auto it = registry_by_client_id_.find(client_id);
    if (it == registry_by_client_id_.end()) {
        return; // Component was not in the registry
    }

    const ComponentInfo& info = it->second;

    // Remove from helper maps first
    auto component_id_pair = std::make_pair(info.group_id, info.id_in_group);
    client_id_by_component_id_.erase(component_id_pair);

    if (info.component_type == discovery_protocol::ComponentType::BRIDGE) {
        // Erase-remove idiom to remove the client_id from the vector
        bridge_client_ids_.erase(
            std::remove(bridge_client_ids_.begin(), bridge_client_ids_.end(), client_id),
            bridge_client_ids_.end()
        );
    } else if (info.component_type == discovery_protocol::ComponentType::OFFLOAD_MANAGER) {
        // Erase-remove idiom to remove the client_id from the vector
        om_client_ids_.erase(
            std::remove(om_client_ids_.begin(), om_client_ids_.end(), client_id),
            om_client_ids_.end()
        );
    }

    // Remove from primary registry
    registry_by_client_id_.erase(it);
}

bool ComponentRegistry::is_id_taken(uint8_t group_id, uint8_t id_in_group) const {
    auto component_id_pair = std::make_pair(group_id, id_in_group);
    return client_id_by_component_id_.count(component_id_pair) > 0;
}

std::optional<ComponentInfo> ComponentRegistry::find_available_bridge() {
    if (bridge_client_ids_.empty()) {
        return std::nullopt;
    }

    // Simple round-robin
    if (next_bridge_idx_ >= bridge_client_ids_.size()) {
        next_bridge_idx_ = 0;
    }

    uint32_t bridge_client_id = bridge_client_ids_[next_bridge_idx_];
    next_bridge_idx_++;

    return find_by_client_id(bridge_client_id);
}

std::optional<ComponentInfo> ComponentRegistry::find_available_om() {
    if (om_client_ids_.empty()) {
        return std::nullopt;
    }

    // Simple round-robin (though typically only one OM)
    if (next_om_idx_ >= om_client_ids_.size()) {
        next_om_idx_ = 0;
    }

    uint32_t om_client_id = om_client_ids_[next_om_idx_];
    next_om_idx_++;

    return find_by_client_id(om_client_id);
}

std::vector<uint32_t> ComponentRegistry::get_all_client_ids() const {
    std::vector<uint32_t> ids;
    ids.reserve(registry_by_client_id_.size());
    for (const auto& pair : registry_by_client_id_) {
        ids.push_back(pair.first);
    }
    return ids;
}

std::optional<ComponentInfo> ComponentRegistry::find_by_client_id(uint32_t client_id) const {
    auto it = registry_by_client_id_.find(client_id);
    if (it != registry_by_client_id_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::vector<ComponentInfo> ComponentRegistry::get_all_components() const {
    std::vector<ComponentInfo> components;
    components.reserve(registry_by_client_id_.size());
    for (const auto& pair : registry_by_client_id_) {
        components.push_back(pair.second);
    }
    return components;
}

bool ComponentRegistry::update_keepalive(uint8_t group_id, uint8_t id_in_group) {
    auto component_id_pair = std::make_pair(group_id, id_in_group);
    auto it_lookup = client_id_by_component_id_.find(component_id_pair);

    if (it_lookup == client_id_by_component_id_.end()) {
        return false; // Component not found
    }

    uint32_t client_id = it_lookup->second;
    auto it_registry = registry_by_client_id_.find(client_id);

    if (it_registry != registry_by_client_id_.end()) {
        it_registry->second.last_seen = std::chrono::steady_clock::now();
        return true;
    }

    return false;
}