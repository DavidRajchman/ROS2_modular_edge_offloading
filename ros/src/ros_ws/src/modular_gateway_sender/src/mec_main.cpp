#include "modular_gateway_sender/gateway_controller.hpp"
#include "modular_gateway_sender/logging_setup.hpp"
#include "rclcpp/rclcpp.hpp"
#include <signal.h>
#include <csignal>
#include "logging/config.h"

// Signal handler to flush logs before termination
void signal_handler(int signum) {
  // Force flush and shutdown logging system
  CppLogging::Config::Shutdown();
  std::exit(signum);
}

// Install abort handler to flush logs
void install_abort_handler() {
  std::signal(SIGABRT, signal_handler);
  std::signal(SIGSEGV, signal_handler);
  std::signal(SIGTERM, signal_handler);
  std::signal(SIGINT, signal_handler);
}

int main(int argc, char * argv[])
{
  // MEC Gateway main executable
  // This executable is specifically designed for MEC (Mobile Edge Computing) gateways
  // with appropriate defaults and logging configuration.

  // 1. Initialize ROS 2
  rclcpp::init(argc, argv);

  // 2. Set up the centralized logging system for MEC component
  //    This MUST be done before any other part of the gateway is constructed.
  gateway::setup_logging("MEC");

  // 3. Install signal handlers to flush logs on crash
  install_abort_handler();

  try {
    // Create the main GatewayController node
    rclcpp::NodeOptions node_options;
    auto controller_node = std::make_shared<gateway::GatewayController>(node_options);
    
    // 5. Initialize components that require shared_from_this()
    controller_node->initialize();
    
    // 6. Spin the node
    rclcpp::spin(controller_node);
  } catch (const std::exception& e) {
    // Force log flush before re-throwing
    CppLogging::Config::Shutdown();
    throw;
  }

  // 7. Shut down ROS 2 and logging
  rclcpp::shutdown();
  CppLogging::Config::Shutdown();
  
  return 0;
}
