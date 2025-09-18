#ifndef CONFIGURE_LOGGER_HPP
#define CONFIGURE_LOGGER_HPP

#include <string>

namespace bridge {

/*
 * Compile-time configuration (set via CMake):
 *
 * Logging mode (exactly one compile definition added by CMake):
 *   - DBRIDGE_LOG_MODE_BINARY  -> binary log file using AsyncWaitFreeProcessor + FileAppender
 *   - DBRIDGE_LOG_MODE_TEXT    -> plain-text / stdout logging
 *
 * Log level configuration:
 *   Two approaches supported:
 *   1) Preset selection via -DBRIDGE_LOG_LEVEL_PRESET=<preset_name>
 *   2) Direct hex mask via -DBRIDGE_LOG_LEVEL=0xBF (when preset not defined)
 *
 * Available presets: NONE, FATAL, ERROR, WARN, INFO, DEBUG, ALL
 * Default: INFO (0x9F) if neither preset nor direct level specified
 *
 * Examples:
 *   cmake -DBRIDGE_LOG_MODE=BINARY -DBRIDGE_LOG_LEVEL_PRESET=DEBUG ..
 *   cmake -DBRIDGE_LOG_MODE=TEXT -DBRIDGE_LOG_LEVEL=0xFF ..
 */

// Preset to hex mask mapping (compile-time resolution)
#ifdef BRIDGE_LOG_LEVEL_PRESET_NONE
    #define BRIDGE_LOG_LEVEL 0x00
#elif defined(BRIDGE_LOG_LEVEL_PRESET_FATAL)
    #define BRIDGE_LOG_LEVEL 0x1F
#elif defined(BRIDGE_LOG_LEVEL_PRESET_ERROR)
    #define BRIDGE_LOG_LEVEL 0x3F
#elif defined(BRIDGE_LOG_LEVEL_PRESET_WARN)
    #define BRIDGE_LOG_LEVEL 0x7F
#elif defined(BRIDGE_LOG_LEVEL_PRESET_INFO)
    #define BRIDGE_LOG_LEVEL 0x9F
#elif defined(BRIDGE_LOG_LEVEL_PRESET_DEBUG)
    #define BRIDGE_LOG_LEVEL 0xBF
#elif defined(BRIDGE_LOG_LEVEL_PRESET_ALL)
    #define BRIDGE_LOG_LEVEL 0xFF
#elif !defined(BRIDGE_LOG_LEVEL)
    // Default to INFO if no preset and no direct level specified
    #define BRIDGE_LOG_LEVEL 0x9F
#endif

/**
 * @brief Configures the CppLogging library for the bridge component.
 *
 * This function chooses the logging backend based on compile-time mode defines.
 * Binary mode = high performance binary log (decode later with decoder tool).
 * Text mode   = immediate text output to console and file.
 *
 * @param component_type Identifier for the component used in log filename (e.g., "bridge").
 */
void configure_logger(const std::string& component_type);

/**
 * @brief Shuts down the logging system cleanly.
 *
 * This function ensures all pending log messages are flushed and the logging
 * system is properly shut down.
 */
void shutdown_logger();

} // namespace bridge

#endif // CONFIGURE_LOGGER_HPP