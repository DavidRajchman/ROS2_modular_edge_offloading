#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include "GlobalConfig.hpp"
#include <string>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <vector>

struct ActiveSession {
    // Unique request ID from VHC's OFFLOAD_REQUEST
    std::string request_id;
    // VHC component_id in "group:id" format (e.g., "60:5")
    std::string mgwcp_component_id;
    // Task ID from global config
    uint32_t task_id;
    // Task name for logging
    std::string task_name;
    // Input message types (VHC→MEC), from global config
    std::vector<uint8_t> input_message_types;
    // Output message types (MEC→VHC), from global config
    std::vector<uint8_t> output_message_types;
    // MEC component_id assigned by OM (e.g., "70:1")
    std::string assigned_mec_id;
    // Last SESSION_KEEPALIVE timestamp (for timeout detection)
    std::chrono::steady_clock::time_point last_keepalive;
    // Session creation timestamp
    std::chrono::steady_clock::time_point created_at;
    
    // VHC data plane connection status
    bool vhc_dp_connected = false;
    // MEC data plane connection status
    bool mec_dp_connected = false;
    // Routing rules created in RoutingTable
    bool routing_rules_created = false;
};

class SessionManager {
public:
    SessionManager() = default;
    
    bool create_session(const std::string& request_id,
                       const std::string& mgwcp_component_id,
                       uint32_t task_id,
                       const std::string& task_name,
                       const GlobalConfig& global_config);
    
    bool remove_session(const std::string& request_id);
    ActiveSession* get_session(const std::string& request_id);
    const ActiveSession* get_session(const std::string& request_id) const;
    std::vector<ActiveSession*> get_sessions_for_mgwcp(const std::string& mgwcp_component_id);
    bool update_keepalive(const std::string& request_id);
    std::vector<std::string> find_expired_sessions(std::chrono::seconds timeout_duration) const;
    bool set_assigned_mec(const std::string& request_id, const std::string& mec_id);
    bool set_vhc_dp_connected(const std::string& request_id, bool connected);
    bool set_mec_dp_connected(const std::string& request_id, bool connected);
    bool set_routing_rules_created(const std::string& request_id, bool created);
    size_t get_active_session_count() const { return sessions_.size(); }
    std::vector<std::string> get_all_request_ids() const;
    size_t cleanup_sessions_for_mgwcp(const std::string& mgwcp_component_id);

private:
    // Map of active sessions: request_id → ActiveSession
    // NOTE: Session deletion on timeout currently disabled
    std::unordered_map<std::string, ActiveSession> sessions_;
};

#endif // SESSION_MANAGER_HPP