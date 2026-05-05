#include "discovery_service.hpp"
#include <iostream>
#include <string>
#include <transport/logging_utils.hpp>

// The port is now defined at compile time, typically via CMakeLists.txt
#ifndef DISCOVERY_SERVICE_PORT
#define DISCOVERY_SERVICE_PORT 9090 // Default fallback port if not defined by build system
#endif

int main(int argc, char** argv) {
    try {
        uint16_t port = DISCOVERY_SERVICE_PORT;
        bool p2p_mode = false;

        // Simple argument parsing
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--p2p") {
                p2p_mode = true;
            } else if (arg == "--port" && i + 1 < argc) {
                port = static_cast<uint16_t>(std::stoi(argv[++i]));
            }
        }
        
        // The logging macros now work correctly because APP_NAME and VERSION are defined
        if (p2p_mode) {
            LOG_INFO("Starting Discovery Service in P2P Matchmaking Mode on port %u...", port);
        } else {
            LOG_INFO("Starting Discovery Service in Networked Mode on port %u...", port);
        }

        DiscoveryService service(port, p2p_mode);
        service.start();

    } catch (const std::exception& e) {
        LOG_ERROR("A critical error occurred: %s", e.what());
        return 1;
    }

    LOG_INFO("Discovery Service has shut down gracefully.");
    return 0;
}