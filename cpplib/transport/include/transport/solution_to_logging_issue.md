# My Understanding of Your Request

You want to modify the logging system in TransportLib to make it compatible across multiple platforms, especially when transitioning between standalone C++ and ROS2 environments. Your core issue is that currently, the logging macros are hardcoded to use fprintf, but you need a solution that can easily switch between different logging backends (fprintf, iostream, ROS2 logging) without changing how client code calls these logging functions.

From your earlier message: "Make a backwards compatible way that will rely on the same function calls. AND change the logging system by only changing the logging macro to make it more compatible with ros2 and also CPP iostream."

## Proposed Solution

We need to create a more flexible logging system that:
1. Maintains the existing LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG macro interfaces
2. Allows the backend implementation to be swapped without changing client code
3. Supports ROS2 logging when used in that environment

Here's my proposed implementation for logging_utils.hpp:

```cpp
#ifndef LOGGING_UTILS_HPP
#define LOGGING_UTILS_HPP

#include <cstdio> // For fprintf
#include <functional>
#include <string>

// Helper macros for stringification
#ifndef STRINGIFY_DETAIL
    #define STRINGIFY_DETAIL(x) #x
#endif
#ifndef STRINGIFY
    #define STRINGIFY(x) STRINGIFY_DETAIL(x)
#endif

// Version defines - these can be overridden in CMakeLists.txt
#ifndef VERSION_MAJOR
#define VERSION_MAJOR 0 // Default if not set by CMake
#endif
#ifndef VERSION_MINOR
#define VERSION_MINOR 0 // Default if not set by CMake
#endif
#ifndef VERSION_PATCH
#define VERSION_PATCH 0 // Default if not set by CMake
#endif

// Application Name define - this should be overridden in CMakeLists.txt for each target
#ifndef APP_NAME
#define APP_NAME "UnknownApp" // Default application name if not set by CMake
#endif

#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

// Fixed width for application name (adjust as needed)
#define APP_NAME_WIDTH 12

// Define log levels
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

// Default to DEBUG level if not specified
#ifndef ACTIVE_LOG_LEVEL
#define ACTIVE_LOG_LEVEL LOG_LEVEL_DEBUG
#endif

namespace gateway {
namespace logging {

// Log handler function type
using LogHandlerFunc = std::function<void(const std::string&)>;

// Global log handlers
extern LogHandlerFunc error_handler;
extern LogHandlerFunc warn_handler;
extern LogHandlerFunc info_handler;
extern LogHandlerFunc debug_handler;

// Initialize with default fprintf handlers
void init_default_handlers();

// Set custom log handlers
void set_log_handlers(
    LogHandlerFunc error_func,
    LogHandlerFunc warn_func,
    LogHandlerFunc info_func,
    LogHandlerFunc debug_func);

// Helper functions to format log messages
std::string format_log_message(const char* level, const char* format, ...);

// Internal logging functions
void log_error(const std::string& message);
void log_warn(const std::string& message);
void log_info(const std::string& message);
void log_debug(const std::string& message);

} // namespace logging
} // namespace gateway

// Logging macros that use the handlers
#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(format, ...) \
    gateway::logging::log_error( \
        gateway::logging::format_log_message("ERROR", format, ##__VA_ARGS__))
#else
#define LOG_ERROR(format, ...) ((void)0)
#endif

#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(format, ...) \
    gateway::logging::log_warn( \
        gateway::logging::format_log_message("WARN", format, ##__VA_ARGS__))
#else
#define LOG_WARN(format, ...) ((void)0)
#endif

#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(format, ...) \
    gateway::logging::log_info( \
        gateway::logging::format_log_message("INFO", format, ##__VA_ARGS__))
#else
#define LOG_INFO(format, ...) ((void)0)
#endif

#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(format, ...) \
    gateway::logging::log_debug( \
        gateway::logging::format_log_message("DEBUG", format, ##__VA_ARGS__))
#else
#define LOG_DEBUG(format, ...) ((void)0)
#endif

#endif // LOGGING_UTILS_HPP
```

And the implementation file:

```cpp
#include "transport/logging_utils.hpp"
#include <cstdarg>
#include <cstring>
#include <iostream>

namespace gateway {
namespace logging {

// Initialize handlers with nullptr
LogHandlerFunc error_handler = nullptr;
LogHandlerFunc warn_handler = nullptr;
LogHandlerFunc info_handler = nullptr;
LogHandlerFunc debug_handler = nullptr;

// Default handlers using fprintf
void default_error_handler(const std::string& message) {
    fprintf(stderr, "%s\n", message.c_str());
}

void default_warn_handler(const std::string& message) {
    fprintf(stderr, "%s\n", message.c_str());
}

void default_info_handler(const std::string& message) {
    fprintf(stdout, "%s\n", message.c_str());
}

void default_debug_handler(const std::string& message) {
    fprintf(stdout, "%s\n", message.c_str());
}

void init_default_handlers() {
    error_handler = default_error_handler;
    warn_handler = default_warn_handler;
    info_handler = default_info_handler;
    debug_handler = default_debug_handler;
}

void set_log_handlers(
    LogHandlerFunc error_func,
    LogHandlerFunc warn_func,
    LogHandlerFunc info_func,
    LogHandlerFunc debug_func)
{
    error_handler = error_func ? error_func : default_error_handler;
    warn_handler = warn_func ? warn_func : default_warn_handler;
    info_handler = info_func ? info_func : default_info_handler;
    debug_handler = debug_func ? debug_func : default_debug_handler;
}

std::string format_log_message(const char* level, const char* format, ...) {
    char buffer[1024]; // Fixed buffer size
    
    // Format the prefix with app name and version
    int prefix_len = snprintf(buffer, sizeof(buffer), 
                             "%*.*s-[" VERSION_STRING "] %s: ", 
                             APP_NAME_WIDTH, APP_NAME_WIDTH, APP_NAME, level);
    
    if (prefix_len < 0 || prefix_len >= static_cast<int>(sizeof(buffer))) {
        return "Error formatting log message prefix";
    }
    
    // Format the user message
    va_list args;
    va_start(args, format);
    int msg_len = vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len, format, args);
    va_end(args);
    
    if (msg_len < 0) {
        return std::string(buffer, prefix_len) + "Error formatting log message";
    }
    
    // Return the formatted string
    return buffer;
}

void log_error(const std::string& message) {
    if (error_handler) {
        error_handler(message);
    } else {
        default_error_handler(message);
    }
}

void log_warn(const std::string& message) {
    if (warn_handler) {
        warn_handler(message);
    } else {
        default_warn_handler(message);
    }
}

void log_info(const std::string& message) {
    if (info_handler) {
        info_handler(message);
    } else {
        default_info_handler(message);
    }
}

void log_debug(const std::string& message) {
    if (debug_handler) {
        debug_handler(message);
    } else {
        default_debug_handler(message);
    }
}

// Initialize default handlers at library load time
namespace {
    struct LogInitializer {
        LogInitializer() {
            init_default_handlers();
        }
    };
    
    static LogInitializer initializer;
}

} // namespace logging
} // namespace gateway
```

## Integration with ROS2

When using in ROS2, you would set up custom handlers during initialization:

```cpp
#include "rclcpp/rclcpp.hpp"
#include "transport/logging_utils.hpp"

void setup_ros2_logging(rclcpp::Logger logger) {
    using namespace gateway::logging;
    
    set_log_handlers(
        // Error handler
        [logger](const std::string& msg) {
            RCLCPP_ERROR(logger, "%s", msg.c_str());
        },
        // Warn handler
        [logger](const std::string& msg) {
            RCLCPP_WARN(logger, "%s", msg.c_str());
        },
        // Info handler
        [logger](const std::string& msg) {
            RCLCPP_INFO(logger, "%s", msg.c_str());
        },
        // Debug handler
        [logger](const std::string& msg) {
            RCLCPP_DEBUG(logger, "%s", msg.c_str());
        }
    );
}

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);
    auto node = rclcpp::Node::make_shared("my_node");
    
    // Setup ROS2 logging
    setup_ros2_logging(node->get_logger());
    
    // Now all LOG_* macros will use ROS2 logging
    LOG_INFO("This will log through ROS2");
    
    // ...rest of your code...
}
```

This solution maintains backward compatibility while providing the flexibility to use different logging backends.