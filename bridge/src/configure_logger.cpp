#include "configure_logger.hpp"
#include "logging/logger.h"
#include "logging/config.h"

namespace bridge {

// Normalize mode into an integer for the required switch statement.
// (Exactly one of BRIDGE_LOG_MODE_BINARY or BRIDGE_LOG_MODE_TEXT should be defined by CMake.)
#if defined(BRIDGE_LOG_MODE_TEXT)
    static constexpr int kCompiledLogMode = 1;
#elif defined(BRIDGE_LOG_MODE_BINARY)
    static constexpr int kCompiledLogMode = 0;
#else
    // Fallback: default to binary if nothing provided.
    static constexpr int kCompiledLogMode = 0;
#endif

// Log level numeric (mask or enum code) passed via -DBRIDGE_LOG_LEVEL=0x9F etc.
#ifndef BRIDGE_LOG_LEVEL
    #define BRIDGE_LOG_LEVEL 0x9F  // Default to INFO if not supplied.
#endif

void configure_logger(const std::string& component_type)
{
    switch (kCompiledLogMode)
    {
        case 0: // Binary logging (current / default behavior)
        {
            // Use AsyncWaitFreeProcessor for optimal multithreaded performance
            auto sink = std::make_shared<CppLogging::AsyncWaitFreeProcessor>(
                std::make_shared<CppLogging::BinaryLayout>(),
                true,     // auto_start
                32768,    // capacity (power of 2)
                false     // block if buffer is full (avoid backpressure stalls)
            );

            std::string log_filename = component_type + "_binary.log";
            sink->appenders().push_back(std::make_shared<CppLogging::FileAppender>(log_filename));

            // Configure the global logger named "bridge"
            CppLogging::Config::ConfigLogger("bridge", sink);

            // (Optional future hook) Apply BRIDGE_LOG_LEVEL mask if the library exposes filtering API.
            // Placeholder: currently we just record the value in an initial log line.

            CppLogging::Config::Startup();

            // Acquire and announce mode/level.
            auto announce_logger = CppLogging::Logger("bridge");
            announce_logger.Info("configure_logger.cpp: Initialized BINARY logging (file='{}', BRIDGE_LOG_LEVEL=0x{:X})",
                                 log_filename, static_cast<unsigned int>(BRIDGE_LOG_LEVEL));
            break;
        }
        case 1: // Text logging
        {
            // Create text layout processor with console and file output
            auto sink = std::make_shared<CppLogging::AsyncWaitFreeProcessor>(
                std::make_shared<CppLogging::TextLayout>(),
                true,     // auto_start
                32768,    // capacity
                false     // non-blocking
            );
            
            // Add console appender for immediate text output
            sink->appenders().push_back(std::make_shared<CppLogging::ConsoleAppender>());
            
            // Also add file appender for text logging
            std::string log_filename = component_type + "_text.log";
            sink->appenders().push_back(std::make_shared<CppLogging::FileAppender>(log_filename));

            // Configure the global logger named "bridge" (same pattern as binary)
            CppLogging::Config::ConfigLogger("bridge", sink);
            CppLogging::Config::Startup();

            // Obtain logger instance using public constructor and announce
            auto announce_logger = CppLogging::Logger("bridge");
            announce_logger.Info("configure_logger.cpp: Initialized TEXT logging (console + file='{}', BRIDGE_LOG_LEVEL=0x{:X})", 
                                 log_filename, static_cast<unsigned int>(BRIDGE_LOG_LEVEL));
            break;
        }
        default:
        {
            // Should not happen; fallback to binary minimal setup.
            auto sink = std::make_shared<CppLogging::AsyncWaitFreeProcessor>(
                std::make_shared<CppLogging::BinaryLayout>(),
                true,
                32768,
                false
            );
            std::string log_filename = component_type + "_binary.log";
            sink->appenders().push_back(std::make_shared<CppLogging::FileAppender>(log_filename));
            CppLogging::Config::ConfigLogger("bridge", sink);
            CppLogging::Config::Startup();
            auto announce_logger = CppLogging::Logger("bridge");
            announce_logger.Warn("configure_logger.cpp: Unknown log mode {}, defaulted to BINARY (BRIDGE_LOG_LEVEL=0x{:X})",
                                 kCompiledLogMode, static_cast<unsigned int>(BRIDGE_LOG_LEVEL));
            break;
        }
    }
}

void shutdown_logger()
{
    CppLogging::Config::Shutdown();
}

} // namespace bridge