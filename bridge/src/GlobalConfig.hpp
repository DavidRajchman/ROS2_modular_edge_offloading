#ifndef GLOBAL_CONFIG_HPP
#define GLOBAL_CONFIG_HPP

#include <nlohmann/json.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>

struct TaskConfig {
    uint32_t task_id;
    std::string task_name;
    std::vector<uint8_t> input_message_types;
    std::vector<uint8_t> output_message_types;
};

class GlobalConfig {
public:
    GlobalConfig() = default;
    
    // Parse global configuration from Discovery Service response
    bool parse_from_json(const nlohmann::json& config_json);
    
    // Fast lookup for route creation during session approval
    const TaskConfig* get_task_config(uint32_t task_id) const;
    
    // Get all available tasks
    const std::vector<TaskConfig>& get_all_tasks() const { return tasks_; }
    
    // Configuration parameters
    uint32_t get_default_session_timeout() const { return default_session_timeout_; }
    uint32_t get_max_concurrent_sessions() const { return max_concurrent_sessions_; }

private:
    std::vector<TaskConfig> tasks_;
    std::unordered_map<uint32_t, size_t> task_id_to_index_;
    uint32_t default_session_timeout_ = 300;
    uint32_t max_concurrent_sessions_ = 100;
};

#endif // GLOBAL_CONFIG_HPP