#include "BridgeControlPlane.hpp"
#include "logging/logger.h"
#include "logging/config.h"

#include <iostream>
#include <csignal>
#include <atomic>
#include <memory>

// Global atomic flag for graceful shutdown
std::atomic<bool> shutdown_requested(false);

// Global Bridge Control Plane instance for signal handler access
std::unique_ptr<BridgeControlPlane> bridge_cp;

void signal_handler(int signum) {
    CppLogging::Logger logger("bridge");
    logger.Info("main.cpp: Received signal {} ({}). Initiating graceful shutdown...", 
               signum, signum == SIGINT ? "SIGINT" : signum == SIGTERM ? "SIGTERM" : "UNKNOWN");
    
    shutdown_requested.store(true);
    
    // Stop the Bridge Control Plane if it's running
    if (bridge_cp && bridge_cp->is_running()) {
        logger.Info("main.cpp: Stopping Bridge Control Plane...");
        bridge_cp->stop();
    }
}

void configure_logger() {
    // Create a binary layout processor for high-performance logging
    auto sink = std::make_shared<CppLogging::Processor>(std::make_shared<CppLogging::BinaryLayout>());
    
    // Add file appender for bridge logs
    sink->appenders().push_back(std::make_shared<CppLogging::FileAppender>("bridge_binary.log"));
    
    // Configure the bridge logger
    CppLogging::Config::ConfigLogger("bridge", sink);
    
    // Startup the logging system
    CppLogging::Config::Startup();
}

int main(int argc, char* argv[]) {
    // Configure and start logging system
    try {
        configure_logger();
    } catch (const std::exception& e) {
        std::cerr << "FATAL: Failed to initialize logging system: " << e.what() << std::endl;
        return 1;
    }
    
    CppLogging::Logger logger("bridge");
    
    // Log startup information
    logger.Info("main.cpp: Bridge Control Plane starting...");
    logger.Info("main.cpp: Component ID: {}", BRIDGE_COMPONENT_ID);
    logger.Info("main.cpp: OM Port: {}", OM_PORT);
    logger.Info("main.cpp: Discovery Service: {}:{}", DISCOVERY_SERVICE_HOST, DISCOVERY_SERVICE_PORT);
    logger.Info("main.cpp: MGWCP Server Port: {}", MGWCP_SERVER_PORT);
    
    // Register signal handlers for graceful shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    logger.Info("main.cpp: Registered signal handlers for graceful shutdown");
    
    int exit_code = 0;
    
    try {
        // Create Bridge Control Plane instance
        logger.Info("main.cpp: Creating Bridge Control Plane instance...");
        bridge_cp = std::make_unique<BridgeControlPlane>();
        
        // Start the Bridge Control Plane
        logger.Info("main.cpp: Starting Bridge Control Plane...");
        if (!bridge_cp->start()) {
            logger.Error("main.cpp: Failed to start Bridge Control Plane");
            exit_code = 1;
        } else {
            logger.Info("main.cpp: Bridge Control Plane started successfully");
            
            // Main loop - wait for shutdown signal
            logger.Info("main.cpp: Bridge Control Plane is operational. Waiting for shutdown signal...");
            while (!shutdown_requested.load() && bridge_cp->is_running()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            // Check why we exited the loop
            if (shutdown_requested.load()) {
                logger.Info("main.cpp: Shutdown requested by signal");
            } else if (!bridge_cp->is_running()) {
                logger.Warn("main.cpp: Bridge Control Plane stopped unexpectedly");
                exit_code = 1;
            }
        }
        
        // Stop the Bridge Control Plane if it's still running
        if (bridge_cp->is_running()) {
            logger.Info("main.cpp: Stopping Bridge Control Plane...");
            bridge_cp->stop();
        }
        
        // Log final status
        logger.Info("main.cpp: Final status - Discovery connected: {}, OM connected: {}, MGWCP connections: {}", 
                   bridge_cp->is_discovery_connected(), 
                   bridge_cp->is_om_connected(), 
                   bridge_cp->get_mgwcp_connection_count());
        
    } catch (const std::exception& e) {
        logger.Error("main.cpp: Fatal error during Bridge Control Plane operation: {}", e.what());
        exit_code = 1;
    } catch (...) {
        logger.Error("main.cpp: Unknown fatal error during Bridge Control Plane operation");
        exit_code = 1;
    }
    
    // Clean up Bridge Control Plane
    if (bridge_cp) {
        logger.Info("main.cpp: Destroying Bridge Control Plane instance...");
        bridge_cp.reset();
    }
    
    // Final log message
    if (exit_code == 0) {
        logger.Info("main.cpp: Bridge Control Plane shutdown completed successfully");
    } else {
        logger.Error("main.cpp: Bridge Control Plane shutdown completed with errors (exit code: {})", exit_code);
    }
    
    // Shutdown logging system
    CppLogging::Config::Shutdown();
    
    return exit_code;
}