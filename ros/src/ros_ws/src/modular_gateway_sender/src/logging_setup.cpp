#include "modular_gateway_sender/logging_setup.hpp"
#include "logging/logger.h"
#include "logging/config.h"

namespace gateway {

// Normalize mode into an integer for the required switch statement.
// (Exactly one of MGW_LOG_MODE_BINARY or MGW_LOG_MODE_TEXT should be defined by CMake.)
#if defined(MGW_LOG_MODE_TEXT)
    static constexpr int kCompiledLogMode = 1;
#elif defined(MGW_LOG_MODE_BINARY)
    static constexpr int kCompiledLogMode = 0;
#else
    // Fallback: default to binary if nothing provided.
    static constexpr int kCompiledLogMode = 0;
#endif

// Log level numeric (mask or enum code) passed via -DMGW_LOG_LEVEL=0x9F etc.
#ifndef MGW_LOG_LEVEL
    #define MGW_LOG_LEVEL 0x9F  // Default to INFO if not supplied.
#endif

void setup_logging(const std::string& component_type)
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

            // Configure the global logger named "gateway"
            CppLogging::Config::ConfigLogger("gateway", sink);

            // (Optional future hook) Apply MGW_LOG_LEVEL mask if the library exposes filtering API.
            // Placeholder: currently we just record the value in an initial log line.

            CppLogging::Config::Startup();

            // Acquire and announce mode/level.
            auto announce_logger = CppLogging::Logger("gateway");
            announce_logger.Info("logging_setup.cpp: Initialized BINARY logging (file='{}', MGW_LOG_LEVEL=0x{:X})",
                                 log_filename, static_cast<unsigned int>(MGW_LOG_LEVEL));
            break;
        }
        case 1: // Text logging (placeholder)
        {
            // TODO: Implement text / console logging mode.
            // Create text layout processor with console output
            auto sink = std::make_shared<CppLogging::AsyncWaitFreeProcessor>(
                std::make_shared<CppLogging::TextLayout>(),
                true,     // auto_start
                32768,    // capacity
                false     // non-blocking
            );
            
            // Add console appender for immediate text output
            sink->appenders().push_back(std::make_shared<CppLogging::ConsoleAppender>());

            // Configure the global logger named "gateway" (same pattern as binary)
            CppLogging::Config::ConfigLogger("gateway", sink);
            CppLogging::Config::Startup();

            // Obtain logger instance using public constructor and announce
            auto announce_logger = CppLogging::Logger("gateway");
            announce_logger.Info("logging_setup.cpp: Initialized TEXT logging (console output, MGW_LOG_LEVEL=0x{:X})", 
                                 static_cast<unsigned int>(MGW_LOG_LEVEL));
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
            CppLogging::Config::ConfigLogger("gateway", sink);
            CppLogging::Config::Startup();
            auto announce_logger = CppLogging::Logger("gateway");
            announce_logger.Warn("logging_setup.cpp: Unknown log mode {}, defaulted to BINARY (MGW_LOG_LEVEL=0x{:X})",
                                 kCompiledLogMode, static_cast<unsigned int>(MGW_LOG_LEVEL));
            break;
        }
    }
}

} // namespace gateway