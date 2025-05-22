#ifndef LOGGING_UTILS_HPP
#define LOGGING_UTILS_HPP

#include <cstdio> // For fprintf

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

// --- Compile-Time Log Level Control ---
#define LOG_LEVEL_NONE  0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_INFO  3
#define LOG_LEVEL_DEBUG 4

/**
 * @brief Define the active log level.
 * All log messages with a level numerically higher than ACTIVE_LOG_LEVEL will be disabled.
 * For example, if ACTIVE_LOG_LEVEL is LOG_LEVEL_INFO, then LOG_DEBUG messages will be compiled out.
 * If ACTIVE_LOG_LEVEL is LOG_LEVEL_NONE, all logs are disabled.
 * This can be overridden in CMakeLists.txt, e.g., target_compile_definitions(my_target PRIVATE ACTIVE_LOG_LEVEL=LOG_LEVEL_INFO)
 */
#ifndef ACTIVE_LOG_LEVEL
#define ACTIVE_LOG_LEVEL LOG_LEVEL_DEBUG // Default: all logs enabled
#endif

/**
 * @brief Enhanced error logging with automatic application name and version tagging.
 * Compiled only if ACTIVE_LOG_LEVEL >= LOG_LEVEL_ERROR.
 */
#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(format, ...) \
    fprintf(stderr, "%*.*s-[" VERSION_STRING "] ERROR: " format "\n", \
            APP_NAME_WIDTH, APP_NAME_WIDTH, APP_NAME, ##__VA_ARGS__)
#else
#define LOG_ERROR(format, ...) ((void)0)
#endif

/**
 * @brief Enhanced warning logging with automatic application name and version tagging.
 * Compiled only if ACTIVE_LOG_LEVEL >= LOG_LEVEL_WARN.
 */
#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(format, ...) \
    fprintf(stderr, "%*.*s-[" VERSION_STRING "] WARN: " format "\n", \
            APP_NAME_WIDTH, APP_NAME_WIDTH, APP_NAME, ##__VA_ARGS__)
#else
#define LOG_WARN(format, ...) ((void)0)
#endif

/**
 * @brief Enhanced info logging with automatic application name and version tagging.
 * Compiled only if ACTIVE_LOG_LEVEL >= LOG_LEVEL_INFO.
 */
#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(format, ...) \
    fprintf(stdout, "%*.*s-[" VERSION_STRING "] INFO: " format "\n", \
            APP_NAME_WIDTH, APP_NAME_WIDTH, APP_NAME, ##__VA_ARGS__)
#else
#define LOG_INFO(format, ...) ((void)0)
#endif

/**
 * @brief Enhanced debug logging with automatic application name and version tagging.
 * Compiled only if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG.
 */
#if ACTIVE_LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(format, ...) \
    fprintf(stdout, "%*.*s-[" VERSION_STRING "] DEBUG: " format "\n", \
            APP_NAME_WIDTH, APP_NAME_WIDTH, APP_NAME, ##__VA_ARGS__)
#else
#define LOG_DEBUG(format, ...) ((void)0)
#endif

#endif // LOGGING_UTILS_HPP