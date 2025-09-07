#ifndef LOGGING_SETUP_HPP
#define LOGGING_SETUP_HPP

#include <string>

namespace gateway {

/*
 * Compile-time configuration (set via CMake):
 *
 * Logging mode (exactly one compile definition added by CMake):
 *   - DMGW_LOG_MODE_BINARY  -> binary log file using AsyncWaitFreeProcessor + FileAppender
 *   - DMGW_LOG_MODE_TEXT    -> (placeholder) future plain-text / stdout logging
 *
 * Log level configuration:
 *   Two approaches supported:
 *   1) Preset selection via -DMGW_LOG_LEVEL_PRESET=<preset_name>
 *   2) Direct hex mask via -DMGW_LOG_LEVEL=0xBF (when preset not defined)
 *
 * Available presets: NONE, FATAL, ERROR, WARN, INFO, DEBUG, ALL
 * Default: INFO (0x9F) if neither preset nor direct level specified
 *
 * Examples:
 *   cmake -DMGW_LOG_MODE=BINARY -DMGW_LOG_LEVEL_PRESET=DEBUG ..
 *   cmake -DMGW_LOG_MODE=TEXT -DMGW_LOG_LEVEL=0xFF ..
 */

// Preset to hex mask mapping (compile-time resolution)
#ifdef MGW_LOG_LEVEL_PRESET_NONE
    #define MGW_LOG_LEVEL 0x00
#elif defined(MGW_LOG_LEVEL_PRESET_FATAL)
    #define MGW_LOG_LEVEL 0x1F
#elif defined(MGW_LOG_LEVEL_PRESET_ERROR)
    #define MGW_LOG_LEVEL 0x3F
#elif defined(MGW_LOG_LEVEL_PRESET_WARN)
    #define MGW_LOG_LEVEL 0x7F
#elif defined(MGW_LOG_LEVEL_PRESET_INFO)
    #define MGW_LOG_LEVEL 0x9F
#elif defined(MGW_LOG_LEVEL_PRESET_DEBUG)
    #define MGW_LOG_LEVEL 0xBF
#elif defined(MGW_LOG_LEVEL_PRESET_ALL)
    #define MGW_LOG_LEVEL 0xFF
#elif !defined(MGW_LOG_LEVEL)
    // Default to INFO if no preset and no direct level specified
    #define MGW_LOG_LEVEL 0x9F
#endif

/**
 * @brief Configures the CppLogging library for a specific component.
 *
 * This function chooses the logging backend based on compile-time mode defines.
 * Binary mode = existing behavior (high performance binary log -> decoder later).
 * Text mode    = placeholder (to be implemented).
 *
 * @param component_type Identifier for the component (e.g., "VHC", "MEC") used in log filename.
 */
void setup_logging(const std::string& component_type);

} // namespace gateway

#endif // LOGGING_SETUP_HPP