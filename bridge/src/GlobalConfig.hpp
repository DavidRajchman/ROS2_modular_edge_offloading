#ifndef GLOBAL_CONFIG_HPP
#define GLOBAL_CONFIG_HPP

#include <nlohmann/json.hpp>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>

// Task definition from Discovery Service available_tasks
struct TaskConfig {
    // Unique task identifier
    uint32_t task_id;
    // Human-readable task name
    std::string task_name;
    // Message types VHC sends to MEC for this task
    std::vector<uint8_t> input_message_types;
    // Message types MEC sends back to VHC for this task
    std::vector<uint8_t> output_message_types;
};

class GlobalConfig {
public:
    GlobalConfig() = default;
    bool parse_from_json(const nlohmann::json& config_json);
    const TaskConfig* get_task_config(uint32_t task_id) const;
    const std::vector<TaskConfig>& get_all_tasks() const { return tasks_; }
    uint32_t get_default_session_timeout() const { return default_session_timeout_; }
    uint32_t get_max_concurrent_sessions() const { return max_concurrent_sessions_; }

private:
    // List of all available tasks from Discovery Service
    std::vector<TaskConfig> tasks_;
    // Fast lookup map: task_id → index in tasks_ vector
    std::unordered_map<uint32_t, size_t> task_id_to_index_;
    // Default session timeout in seconds (default 300)
    uint32_t default_session_timeout_ = 300;
    // Maximum concurrent sessions allowed (default 100)
    uint32_t max_concurrent_sessions_ = 100;
};

#endif // GLOBAL_CONFIG_HPP