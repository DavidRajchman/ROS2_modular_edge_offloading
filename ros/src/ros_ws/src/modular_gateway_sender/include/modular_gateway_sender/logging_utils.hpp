/**
 * @file logging_utils.hpp
 * @brief LEGACY - DO NOT USE FOR NEW FILES Common logging utilities and version macros for the modular gateway system.
 * 
 * This file provides versioned logging macros that automatically include version information
 * in all log messages, making it easier to track which version produced specific logs.
 * It also includes compile-time log level control.
 */

 #ifndef LOGGING_UTILS_HPP
 #define LOGGING_UTILS_HPP
 
 #include "rclcpp/rclcpp.hpp"
 
 // Version defines - these can be overridden in CMakeLists.txt
 #ifndef VERSION_MAJOR
 #define VERSION_MAJOR 0
 #endif
 
 #ifndef VERSION_MINOR
 #define VERSION_MINOR 4
 #endif
 
 #ifndef VERSION_PATCH
 #define VERSION_PATCH 0
 #endif
 
 // Macro magic to convert defines to string
 #define STRINGIFY_HELPER(x) #x
 #define STRINGIFY(x) STRINGIFY_HELPER(x)
 
 /**
  * @brief Combined version string in format "MAJOR.MINOR.PATCH"
  * 
  * Used in logging and potentially for protocol negotiation.
  */
 #define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)
 
 // --- Compile-Time Log Level Control ---
 #define LOG_LEVEL_NONE  0
 #define LOG_LEVEL_ERROR 1
 #define LOG_LEVEL_WARN  2
 #define LOG_LEVEL_INFO  3
 #define LOG_LEVEL_DEBUG 4
 
 /**
  * @brief Define the active log level for compile-time filtering.
  * All log messages with a level numerically higher than ACTIVE_LOG_LEVEL will be disabled
  * (compiled out). For example, if ACTIVE_LOG_LEVEL is LOG_LEVEL_INFO, then
  * LOG_DEBUG messages will be compiled out. If ACTIVE_LOG_LEVEL is LOG_LEVEL_NONE,
  * all custom logs (LOG_INFO, LOG_WARN, LOG_ERROR, LOG_DEBUG) are disabled.
  * This can be overridden in CMakeLists.txt, e.g., 
  * target_compile_definitions(my_target PRIVATE ACTIVE_LOG_LEVEL=LOG_LEVEL_INFO)
  */
 #ifndef ACTIVE_LOG_LEVEL
 #define ACTIVE_LOG_LEVEL LOG_LEVEL_WARN // Default: all logs enabled
 #endif
 
 /**
  * @brief Enhanced info logging with automatic version tagging and compile-time level check.
  * 
  * Prefixes all log messages with the current version for easier debugging.
  * Compiled only if ACTIVE_LOG_LEVEL >= LOG_LEVEL_INFO.
  * 
  * @param logger The ROS logger instance
  * @param format Format string for the message
  * @param ... Format string arguments
  */
 #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_INFO
 #define LOG_INFO(logger, format, ...) \
     RCLCPP_INFO(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)
 #else
 #define LOG_INFO(logger, format, ...) ((void)0)
 #endif
 
 /**
  * @brief Enhanced warning logging with automatic version tagging and compile-time level check.
  * 
  * Prefixes all log messages with the current version for easier debugging.
  * Compiled only if ACTIVE_LOG_LEVEL >= LOG_LEVEL_WARN.
  * 
  * @param logger The ROS logger instance
  * @param format Format string for the message
  * @param ... Format string arguments
  */
 #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_WARN
 #define LOG_WARN(logger, format, ...) \
     RCLCPP_WARN(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)
 #else
 #define LOG_WARN(logger, format, ...) ((void)0)
 #endif
 
 /**
  * @brief Enhanced error logging with automatic version tagging and compile-time level check.
  * 
  * Prefixes all log messages with the current version for easier debugging.
  * Compiled only if ACTIVE_LOG_LEVEL >= LOG_LEVEL_ERROR.
  * 
  * @param logger The ROS logger instance
  * @param format Format string for the message
  * @param ... Format string arguments
  */
 #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_ERROR
 #define LOG_ERROR(logger, format, ...) \
     RCLCPP_ERROR(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)
 #else
 #define LOG_ERROR(logger, format, ...) ((void)0)
 #endif
 
 /**
  * @brief Enhanced debug logging with automatic version tagging and compile-time level check.
  * 
  * Prefixes all log messages with the current version for easier debugging.
  * Compiled only if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG.
  * 
  * @param logger The ROS logger instance
  * @param format Format string for the message
  * @param ... Format string arguments
  */
 #if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG
 #define LOG_DEBUG(logger, format, ...) \
     RCLCPP_DEBUG(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)
 #else
 #define LOG_DEBUG(logger, format, ...) ((void)0)
 #endif
 
 #endif // LOGGING_UTILS_HPP