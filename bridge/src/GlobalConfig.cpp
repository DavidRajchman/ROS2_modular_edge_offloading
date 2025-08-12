#include "GlobalConfig.hpp"
#include "logging/logger.h" // For CppLogging

bool GlobalConfig::parse_from_json(const nlohmann::json& config_json) {
    CppLogging::Logger logger("bridge");
    
    try {
        // Clear existing configuration
        tasks_.clear();
        task_id_to_index_.clear();
        
        // Parse available tasks
        if (config_json.contains("available_tasks")) {
            const auto& tasks_array = config_json["available_tasks"];
            tasks_.reserve(tasks_array.size());
            
            for (size_t i = 0; i < tasks_array.size(); ++i) {
                const auto& task_json = tasks_array[i];
                
                TaskConfig task;
                task.task_id = task_json["task_id"];
                task.task_name = task_json["task_name"];
                
                // Parse input message types
                if (task_json.contains("input_message_types")) {
                    for (const auto& msg_type : task_json["input_message_types"]) {
                        task.input_message_types.push_back(static_cast<uint8_t>(msg_type));
                    }
                }
                
                // Parse output message types
                if (task_json.contains("output_message_types")) {
                    for (const auto& msg_type : task_json["output_message_types"]) {
                        task.output_message_types.push_back(static_cast<uint8_t>(msg_type));
                    }
                }
                
                // Store task and create index mapping
                task_id_to_index_[task.task_id] = tasks_.size();
                tasks_.push_back(std::move(task));
                
                logger.Info("GlobalConfig: Parsed task {} ({}) with {} input types and {} output types",
                    task.task_id, task.task_name,
                    task.input_message_types.size(),
                    task.output_message_types.size());
            }
        }
        
        // Parse optional configuration parameters
        if (config_json.contains("default_session_timeout")) {
            default_session_timeout_ = config_json["default_session_timeout"] + 10;
        }
        
        if (config_json.contains("max_concurrent_sessions")) {
            max_concurrent_sessions_ = config_json["max_concurrent_sessions"];
        }
        
        logger.Info("GlobalConfig: Successfully parsed {} tasks, timeout={} + 10 seconds, max_sessions={}",
            tasks_.size(), default_session_timeout_, max_concurrent_sessions_);
        
        return true;
        
    } catch (const std::exception& e) {
        logger.Error("GlobalConfig: Failed to parse configuration JSON: {}", e.what());
        return false;
    }
}

const TaskConfig* GlobalConfig::get_task_config(uint32_t task_id) const {
    auto it = task_id_to_index_.find(task_id);
    if (it != task_id_to_index_.end()) {
        return &tasks_[it->second];
    }
    return nullptr;
}