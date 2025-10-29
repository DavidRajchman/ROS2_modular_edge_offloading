#include "SessionManager.hpp"
#include "logging/logger.h"
#include <algorithm>

//SESSION MANAGER CURRENTLY DISABLED (timeout deletion not active)

// Create new offloading session
// Looks up task config from global_config to get input/output message types
// Initializes keepalive timestamp, returns false if session already exists or task not found
bool SessionManager::create_session(const std::string& request_id,
                                   const std::string& mgwcp_component_id,
                                   uint32_t task_id,
                                   const std::string& task_name,
                                   const GlobalConfig& global_config) {
    CppLogging::Logger logger("bridge");
    
    // Check if session already exists
    if (sessions_.find(request_id) != sessions_.end()) {
        logger.Warn("SessionManager.cpp: Session with request_id '{}' already exists", request_id);
        return false;
    }
    
    // Look up task configuration
    const TaskConfig* task_config = global_config.get_task_config(task_id);
    if (!task_config) {
        logger.Error("SessionManager.cpp: Task ID {} not found in global configuration", task_id);
        return false;
    }
    
    // Create new session
    ActiveSession session;
    session.request_id = request_id;
    session.mgwcp_component_id = mgwcp_component_id;
    session.task_id = task_id;
    session.task_name = task_name;
    session.input_message_types = task_config->input_message_types;
    session.output_message_types = task_config->output_message_types;
    session.last_keepalive = std::chrono::steady_clock::now();
    session.created_at = std::chrono::steady_clock::now();
    
    // Store session
    sessions_[request_id] = std::move(session);
    
    logger.Info("SessionManager.cpp: Created session '{}' for MGWCP '{}', task {} ({}) with {} input types and {} output types",
        request_id, mgwcp_component_id, task_id, task_name,
        task_config->input_message_types.size(),
        task_config->output_message_types.size());
    
    return true;
}

// Remove session from active sessions map
// Called on SESSION_DENIED or explicit termination, returns false if session not found
bool SessionManager::remove_session(const std::string& request_id) {
    CppLogging::Logger logger("bridge");
    
    auto it = sessions_.find(request_id);
    if (it == sessions_.end()) {
        logger.Warn("SessionManager.cpp: Cannot remove session '{}' - not found", request_id);
        return false;
    }
    
    logger.Info("SessionManager.cpp: Removing session '{}' for MGWCP '{}'", 
        request_id, it->second.mgwcp_component_id);
    
    sessions_.erase(it);
    return true;
}

// Get mutable session by request_id, returns nullptr if not found
ActiveSession* SessionManager::get_session(const std::string& request_id) {
    auto it = sessions_.find(request_id);
    return (it != sessions_.end()) ? &it->second : nullptr;
}

// Get const session by request_id, returns nullptr if not found
const ActiveSession* SessionManager::get_session(const std::string& request_id) const {
    auto it = sessions_.find(request_id);
    return (it != sessions_.end()) ? &it->second : nullptr;
}

// Get all sessions for specific VHC/MEC component_id
// Returns vector of pointers to sessions matching mgwcp_component_id
std::vector<ActiveSession*> SessionManager::get_sessions_for_mgwcp(const std::string& mgwcp_component_id) {
    std::vector<ActiveSession*> result;
    
    for (auto& pair : sessions_) {
        if (pair.second.mgwcp_component_id == mgwcp_component_id) {
            result.push_back(&pair.second);
        }
    }
    
    return result;
}

// Update session keepalive timestamp
// Called when SESSION_KEEPALIVE received from VHC, returns false if session not found
bool SessionManager::update_keepalive(const std::string& request_id) {
    CppLogging::Logger logger("bridge");
    
    auto it = sessions_.find(request_id);
    if (it == sessions_.end()) {
        logger.Warn("SessionManager.cpp: Cannot update keepalive for session '{}' - not found", request_id);
        return false;
    }
    
    it->second.last_keepalive = std::chrono::steady_clock::now();
    logger.Debug("SessionManager.cpp: Updated keepalive for session '{}'", request_id);
    return true;
}

// Find sessions with expired keepalive
// Checks last_keepalive against timeout_duration (default 20 seconds)
// NOTE: Currently disabled - expired sessions NOT added to return vector (commented out)
std::vector<std::string> SessionManager::find_expired_sessions(std::chrono::seconds timeout_duration) const {
    CppLogging::Logger logger("bridge");
    std::vector<std::string> expired_sessions;
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& pair : sessions_) {
        const ActiveSession& session = pair.second;
        auto time_since_keepalive = now - session.last_keepalive;
        
        if (time_since_keepalive > timeout_duration) {
            //expired_sessions.push_back(session.request_id);  // DISABLED: Sessions not deleted on timeout
            logger.Debug("SessionManager.cpp: Found expired session '{}' (last keepalive {} seconds ago)",
                session.request_id, 
                std::chrono::duration_cast<std::chrono::seconds>(time_since_keepalive).count());
        }
    }
    
    return expired_sessions;
}

// Set assigned MEC component_id for session
// Called when SESSION_APPROVED received from OM with assigned_mec_id
bool SessionManager::set_assigned_mec(const std::string& request_id, const std::string& mec_id) {
    CppLogging::Logger logger("bridge");
    
    auto it = sessions_.find(request_id);
    if (it == sessions_.end()) {
        logger.Warn("SessionManager.cpp: Cannot set assigned MEC for session '{}' - not found", request_id);
        return false;
    }
    
    it->second.assigned_mec_id = mec_id;
    logger.Info("SessionManager.cpp: Set assigned MEC '{}' for session '{}'", mec_id, request_id);
    return true;
}

// Set VHC data plane connection status
// Tracks whether TransportHandler connected to VHC's data plane
bool SessionManager::set_vhc_dp_connected(const std::string& request_id, bool connected) {
    CppLogging::Logger logger("bridge");
    
    auto it = sessions_.find(request_id);
    if (it == sessions_.end()) {
        logger.Warn("SessionManager.cpp: Cannot set VHC DP status for session '{}' - not found", request_id);
        return false;
    }
    
    it->second.vhc_dp_connected = connected;
    logger.Debug("SessionManager.cpp: Set VHC DP connection status to {} for session '{}'", 
        connected ? "connected" : "disconnected", request_id);
    return true;
}

// Set MEC data plane connection status
// Tracks whether TransportHandler connected to MEC's data plane
bool SessionManager::set_mec_dp_connected(const std::string& request_id, bool connected) {
    CppLogging::Logger logger("bridge");
    
    auto it = sessions_.find(request_id);
    if (it == sessions_.end()) {
        logger.Warn("SessionManager.cpp: Cannot set MEC DP status for session '{}' - not found", request_id);
        return false;
    }
    
    it->second.mec_dp_connected = connected;
    logger.Debug("SessionManager.cpp: Set MEC DP connection status to {} for session '{}'", 
        connected ? "connected" : "disconnected", request_id);
    return true;
}

// Set routing rules created status
// Tracks whether VHC↔MEC bidirectional routes added to RoutingTable
bool SessionManager::set_routing_rules_created(const std::string& request_id, bool created) {
    CppLogging::Logger logger("bridge");
    
    auto it = sessions_.find(request_id);
    if (it == sessions_.end()) {
        logger.Warn("SessionManager.cpp: Cannot set routing rules status for session '{}' - not found", request_id);
        return false;
    }
    
    it->second.routing_rules_created = created;
    logger.Debug("SessionManager.cpp: Set routing rules status to {} for session '{}'", 
        created ? "created" : "not created", request_id);
    return true;
}

// Get all active session request_ids
// Returns vector of all request_ids currently in sessions map
std::vector<std::string> SessionManager::get_all_request_ids() const {
    std::vector<std::string> request_ids;
    request_ids.reserve(sessions_.size());
    
    for (const auto& pair : sessions_) {
        request_ids.push_back(pair.first);
    }
    
    return request_ids;
}

// Cleanup all sessions for disconnected VHC/MEC
// Called by MGWCPConnection::stop() to remove sessions when gateway disconnects
// Returns count of sessions removed
size_t SessionManager::cleanup_sessions_for_mgwcp(const std::string& mgwcp_component_id) {
    CppLogging::Logger logger("bridge");
    size_t removed_count = 0;
    
    auto it = sessions_.begin();
    while (it != sessions_.end()) {
        if (it->second.mgwcp_component_id == mgwcp_component_id) {
            logger.Info("SessionManager.cpp: Cleaning up session '{}' for disconnected MGWCP '{}'",
                it->second.request_id, mgwcp_component_id);
            it = sessions_.erase(it);
            removed_count++;
        } else {
            ++it;
        }
    }
    
    if (removed_count > 0) {
        logger.Info("SessionManager.cpp: Cleaned up {} sessions for MGWCP '{}'", 
            removed_count, mgwcp_component_id);
    }
    
    return removed_count;
}