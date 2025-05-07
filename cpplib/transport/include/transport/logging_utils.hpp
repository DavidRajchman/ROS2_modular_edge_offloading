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
// Change this to expect a string literal from CMake directly
#ifndef APP_NAME
#define APP_NAME "UnknownApp" // Default application name if not set by CMake
#endif

#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

// Fixed width for application name (adjust as needed)
#define APP_NAME_WIDTH 12

/**
 * @brief Enhanced info logging with automatic application name and version tagging.
 * The application name will have a consistent width in the output.
 */
#define LOG_INFO(format, ...) \
    fprintf(stdout, "%*.*s-[" VERSION_STRING "] INFO: " format "\n", \
            APP_NAME_WIDTH, APP_NAME_WIDTH, APP_NAME, ##__VA_ARGS__)

/**
 * @brief Enhanced warning logging with automatic application name and version tagging.
 * The application name will have a consistent width in the output.
 */
#define LOG_WARN(format, ...) \
    fprintf(stderr, "%*.*s-[" VERSION_STRING "] WARN: " format "\n", \
            APP_NAME_WIDTH, APP_NAME_WIDTH, APP_NAME, ##__VA_ARGS__)

/**
 * @brief Enhanced error logging with automatic application name and version tagging.
 * The application name will have a consistent width in the output.
 */
#define LOG_ERROR(format, ...) \
    fprintf(stderr, "%*.*s-[" VERSION_STRING "] ERROR: " format "\n", \
            APP_NAME_WIDTH, APP_NAME_WIDTH, APP_NAME, ##__VA_ARGS__)

/**
 * @brief Enhanced debug logging with automatic application name and version tagging.
 * The application name will have a consistent width in the output.
 */
#define LOG_DEBUG(format, ...) \
    fprintf(stdout, "%*.*s-[" VERSION_STRING "] DEBUG: " format "\n", \
            APP_NAME_WIDTH, APP_NAME_WIDTH, APP_NAME, ##__VA_ARGS__)

#endif // LOGGING_UTILS_HPP