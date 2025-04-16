/**
 * @file logging_utils.hpp
 * @brief Common logging utilities and version macros for the modular gateway system.
 * 
 * This file provides versioned logging macros that automatically include version information
 * in all log messages, making it easier to track which version produced specific logs.
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

/**
 * @brief Enhanced info logging with automatic version tagging.
 * 
 * Prefixes all log messages with the current version for easier debugging.
 * 
 * @param logger The ROS logger instance
 * @param format Format string for the message
 * @param ... Format string arguments
 */
#define LOG_INFO(logger, format, ...) \
    RCLCPP_INFO(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

/**
 * @brief Enhanced warning logging with automatic version tagging.
 * 
 * Prefixes all log messages with the current version for easier debugging.
 * 
 * @param logger The ROS logger instance
 * @param format Format string for the message
 * @param ... Format string arguments
 */
#define LOG_WARN(logger, format, ...) \
    RCLCPP_WARN(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

/**
 * @brief Enhanced error logging with automatic version tagging.
 * 
 * Prefixes all log messages with the current version for easier debugging.
 * 
 * @param logger The ROS logger instance
 * @param format Format string for the message
 * @param ... Format string arguments
 */
#define LOG_ERROR(logger, format, ...) \
    RCLCPP_ERROR(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

/**
 * @brief Enhanced debug logging with automatic version tagging.
 * 
 * Prefixes all log messages with the current version for easier debugging.
 * 
 * @param logger The ROS logger instance
 * @param format Format string for the message
 * @param ... Format string arguments
 */
#define LOG_DEBUG(logger, format, ...) \
    RCLCPP_DEBUG(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

#endif // LOGGING_UTILS_HPP