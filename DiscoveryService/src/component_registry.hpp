#pragma once

#include <discovery_protocol/protocol.hpp>
#include <string>
#include <cstdint>
#include <chrono>
#include <vector>
#include <optional>
#include <unordered_map>
#include <map>

// Represents a component that has successfully registered with the service.
struct ComponentInfo {
    uint32_t client_id; // The ID assigned by the transport layer
    discovery_protocol::ComponentType component_type;
    uint8_t group_id;
    uint8_t id_in_group;
    std::string name;
    std::string listen_address;
    uint16_t listen_port;
    std::chrono::steady_clock::time_point last_seen;
};

// Manages the collection of all registered components.
class ComponentRegistry {
public:
    // Adds a new component to the registry. Returns false if the ID is taken.
    bool register_component(const ComponentInfo& info);

    // Removes a component from the registry using its transport client_id.
    void unregister_component(uint32_t client_id);

    // Checks if a specific component ID (group.id) is already in use.
    bool is_id_taken(uint8_t group_id, uint8_t id_in_group) const;

    // Finds an available Bridge for a VHC to connect to (e.g., round-robin).
    std::optional<ComponentInfo> find_available_bridge();

    // Finds a component by its transport client_id.
    std::optional<ComponentInfo> find_by_client_id(uint32_t client_id) const;

    // Gets a list of all currently registered client IDs.
    std::vector<uint32_t> get_all_client_ids() const;
    
    // Updates the keepalive timestamp for a component.
    bool update_keepalive(uint8_t group_id, uint8_t id_in_group);

private:
    // The primary registry mapping the transport's client_id to component info.
    std::unordered_map<uint32_t, ComponentInfo> registry_by_client_id_;

    // Helper map for quick lookups to check for ID conflicts.
    // Key: A pair of {group_id, id_in_group}, Value: The transport client_id.
    std::map<std::pair<uint8_t, uint8_t>, uint32_t> client_id_by_component_id_;

    // Helper list to quickly find all registered bridges for round-robin assignment.
    std::vector<uint32_t> bridge_client_ids_;
    size_t next_bridge_idx_ = 0;
};