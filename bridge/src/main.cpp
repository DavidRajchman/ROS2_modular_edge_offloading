#include <transport/logging_utils.hpp> // For logging
#include "common_types.hpp"      // For Message, RoutingKey, MPSCQueueType
#include "RoutingTable.hpp"      // For RoutingTable

#include <iostream>
#include <string>
#include <vector>
#include <thread>   // For std::thread, std::this_thread
#include <chrono>   // For std::chrono
#include <atomic>   // For std::atomic_bool for shutdown
#include <csignal>  // For signal handling (Ctrl+C)
#include <memory>   // For std::shared_ptr, std::make_shared

// Global atomic flag for graceful shutdown
std::atomic<bool> shutdown_flag(false);

void signal_handler(int signum) {
    LOG_INFO("Interrupt signal (%d) received.", signum);
    shutdown_flag.store(true);
}

// Dedicated setup function for bridge components
std::shared_ptr<RoutingTable> setup_bridge_components() {
    LOG_INFO("Setting up Bridge components...");

    // Configuration (could be loaded from a file later)
    const size_t MAX_EXPECTED_ROUTES = 10000; // As per our discussion

    // 1. Initialize the Routing Table
    LOG_INFO("Initializing Routing Table with capacity for %zu routes...", MAX_EXPECTED_ROUTES);
    auto routing_table = std::make_shared<RoutingTable>(MAX_EXPECTED_ROUTES);
    LOG_INFO("Routing Table initialized.");

    // 2. Add some basic testing routes (Control Plane will do this later)
    LOG_INFO("Adding initial test routes...");
    auto test_queue_vhc1_to_mec1 = std::make_shared<MPSCQueueType>();
    auto test_queue_mec1_to_vhc1 = std::make_shared<MPSCQueueType>();
    auto test_queue_vhc2_to_mec2 = std::make_shared<MPSCQueueType>();

    RoutingKey key_vhc1_gps = {"VHC1", "GPS_DATA"};
    RoutingKey key_vhc1_cam = {"VHC1", "CAMERA_FEED"};
    RoutingKey key_mec1_res = {"MEC1", "TASK_RESULT"};
    RoutingKey key_vhc2_sen = {"VHC2", "SENSOR_DATA"};

    routing_table->add_route_for_testing(key_vhc1_gps, test_queue_vhc1_to_mec1);
    routing_table->add_route_for_testing(key_vhc1_cam, test_queue_vhc1_to_mec1); // VHC1 CAM also goes to MEC1
    routing_table->add_route_for_testing(key_mec1_res, test_queue_mec1_to_vhc1);
    routing_table->add_route_for_testing(key_vhc2_sen, test_queue_vhc2_to_mec2);
    LOG_INFO("Initial test routes added.");

    // --- Placeholder for future components to be initialized here ---
    // e.g., ControlPlane, DataPlane listeners etc.

    LOG_INFO("Bridge components setup complete.");
    return routing_table;
}

int main(int argc, char* argv[]) {
    // Initialize logging (assuming LOG_INFO, etc. are set up)
    // If your logging_utils needs explicit init, do it here.
    LOG_INFO("Bridge Application Starting...");
    LOG_INFO("--------------------------------");

    // Register signal handler for Ctrl+C
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Setup core components
    std::shared_ptr<RoutingTable> routing_table = setup_bridge_components();

    if (!routing_table) {
        LOG_ERROR("Failed to setup bridge components. Exiting.");
        return 1;
    }

    // --- Placeholder for starting components that run in their own threads ---
    // e.g., control_plane.start();
    //       vhc_listener.start_listening();

    LOG_INFO("Bridge is running. Press Ctrl+C to exit.");
    LOG_INFO("--------------------------------");

    // Main application loop (waiting for shutdown)
    while (!shutdown_flag.load()) {
        // This loop can be used for periodic tasks if any, or just sleep.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    LOG_INFO("--------------------------------");
    LOG_INFO("Bridge Application Shutting Down...");

    // --- Placeholder for graceful shutdown of components ---
    // e.g., control_plane.stop();
    //       vhc_listener.stop();
    LOG_INFO("Initiating shutdown of components (placeholder)...");


    LOG_INFO("Routing Table will be cleared upon its shared_ptr destruction if no other references exist.");
    LOG_INFO("Bridge shutdown complete.");

    return 0;
}