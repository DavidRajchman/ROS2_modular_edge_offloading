#include "discovery_service.hpp"
#include <iostream>
#include <string>
#include <transport/logging_utils.hpp>

// The port is now defined at compile time, typically via CMakeLists.txt
#ifndef DISCOVERY_SERVICE_PORT
#define DISCOVERY_SERVICE_PORT 9090 // Default fallback port if not defined by build system
#endif

int main() {
    try {
        uint16_t port = DISCOVERY_SERVICE_PORT;
        
        // The logging macros now work correctly because APP_NAME and VERSION are defined
        LOG_INFO("Attempting to start Discovery Service...");

        DiscoveryService service(port);
        service.start();

    } catch (const std::exception& e) {
        LOG_ERROR("A critical error occurred: %s", e.what());
        return 1;
    }

    LOG_INFO("Discovery Service has shut down gracefully.");
    return 0;
}