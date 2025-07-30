#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include "GlobalConfig.hpp"
#include <string>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <vector>

struct ActiveSession {
    std::string request_id;           // From OFFLOAD_REQUEST
    std::string mgwcp_component_id;   // "group_id:id_in_group" format
    uint32_t task_id;                 // Task being offloaded
    std::string task_name;            // Task name for logging
    std::vector<uint8_t> input_message_types;   // For VHC→MEC routing
    std::vector<uint8_t> output_message_types;  // For MEC→VHC routing
    std::string assigned_mec_id;      // Assigned MEC component ID (from OM)
    std::chrono::steady_clock::time_point last_keepalive;  // Last keepalive timestamp
    std::chrono::steady_clock::time_point created_at;      // Session creation time
    
    // Data plane connection status
    bool vhc_dp_connected = false;
    bool mec_dp_connected = false;
    bool routing_rules_created = false;
};

class SessionManager {
public:
    SessionManager() = default;
    
    // Session lifecycle management
    bool create_session(const std::string& request_id,
                       const std::string& mgwcp_component_id,
                       uint32_t task_id,
                       const std::string& task_name,
                       const GlobalConfig& global_config);
    
    bool remove_session(const std::string& request_id);
    
    // Session lookup and management
    ActiveSession* get_session(const std::string& request_id);
    const ActiveSession* get_session(const std::string& request_id) const;
    
    std::vector<ActiveSession*> get_sessions_for_mgwcp(const std::string& mgwcp_component_id);
    
    // Keepalive management
    bool update_keepalive(const std::string& request_id);
    std::vector<std::string> find_expired_sessions(std::chrono::seconds timeout_duration) const;
    
    // Session state updates
    bool set_assigned_mec(const std::string& request_id, const std::string& mec_id);
    bool set_vhc_dp_connected(const std::string& request_id, bool connected);
    bool set_mec_dp_connected(const std::string& request_id, bool connected);
    bool set_routing_rules_created(const std::string& request_id, bool created);
    
    // Statistics and monitoring
    size_t get_active_session_count() const { return sessions_.size(); }
    std::vector<std::string> get_all_request_ids() const;
    
    // Cleanup operations
    size_t cleanup_sessions_for_mgwcp(const std::string& mgwcp_component_id);

private:
    std::unordered_map<std::string, ActiveSession> sessions_;  // request_id → session
};

#endif // SESSION_MANAGER_HPP