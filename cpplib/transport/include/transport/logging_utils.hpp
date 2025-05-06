/**
 * @file logging_utils.hpp
 * @brief Common logging utilities and version macros for the modular gateway system.
 * 
 * This file provides versioned logging macros that automatically include version information
 * in all log messages, making it easier to track which version produced specific logs.
 */

#ifndef LOGGING_UTILS_HPP
#define LOGGING_UTILS_HPP

// #include "rclcpp/rclcpp.hpp" // Remove ROS 2 header
#include <cstdio> // Add for fprintf

// Helper macros for stringification
#ifndef STRINGIFY_DETAIL
    #define STRINGIFY_DETAIL(x) #x
#endif
#ifndef STRINGIFY
    #define STRINGIFY(x) STRINGIFY_DETAIL(x)
#endif

// Version defines - these can be overridden in CMakeLists.txt
#ifndef VERSION_MAJOR
#define VERSION_MAJOR 0
#endif
#ifndef VERSION_MINOR
#define VERSION_MINOR 0
#endif
#ifndef VERSION_PATCH
#define VERSION_PATCH 0
#endif

#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

/**
 * @brief Enhanced info logging with automatic version tagging.
 * 
 * Prefixes all log messages with the current version for easier debugging.
 * 
 * @param format Format string for the message
 * @param ... Format string arguments
 */
#define LOG_INFO(format, ...) \
    fprintf(stdout, "[" VERSION_STRING "] INFO: " format "\n", ##__VA_ARGS__)

/**
 * @brief Enhanced warning logging with automatic version tagging.
 * 
 * Prefixes all log messages with the current version for easier debugging.
 * 
 * @param format Format string for the message
 * @param ... Format string arguments
 */
#define LOG_WARN(format, ...) \
    fprintf(stderr, "[" VERSION_STRING "] WARN: " format "\n", ##__VA_ARGS__)

/**
 * @brief Enhanced error logging with automatic version tagging.
 * 
 * Prefixes all log messages with the current version for easier debugging.
 * 
 * @param format Format string for the message
 * @param ... Format string arguments
 */
#define LOG_ERROR(format, ...) \
    fprintf(stderr, "[" VERSION_STRING "] ERROR: " format "\n", ##__VA_ARGS__)

/**
 * @brief Enhanced debug logging with automatic version tagging.
 * 
 * Prefixes all log messages with the current version for easier debugging.
 * 
 * @param format Format string for the message
 * @param ... Format string arguments
 */
#define LOG_DEBUG(format, ...) \
    fprintf(stdout, "[" VERSION_STRING "] DEBUG: " format "\n", ##__VA_ARGS__)

#endif // LOGGING_UTILS_HPP