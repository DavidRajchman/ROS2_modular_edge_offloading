#include "modular_gateway_sender/logging_setup.hpp"
#include "logging/logger.h"
#include "logging/config.h"

namespace gateway {

void setup_logging(const std::string& component_type) {
    // Use AsyncWaitFreeProcessor for optimal multithreaded performance
    auto sink = std::make_shared<CppLogging::AsyncWaitFreeProcessor>(
        std::make_shared<CppLogging::BinaryLayout>(),
        true,    // auto_start
        32768,   // capacity (must be a power of 2)
        false    // block if buffer is full (prevents message loss)
    );
    
    std::string log_filename = component_type + "_binary.log";
    sink->appenders().push_back(
        std::make_shared<CppLogging::FileAppender>(log_filename));
    
    // Configure the global logger named "gateway"
    CppLogging::Config::ConfigLogger("gateway", sink);

    // Start the logging system
    CppLogging::Config::Startup();
}

} // namespace gateway

