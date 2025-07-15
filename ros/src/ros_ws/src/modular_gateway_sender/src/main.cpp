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
  // This is the single entry point for any component running the GatewayController.
  // The specific behavior (VHC, MEC, etc.) is determined by the parameters
  // passed to the node at runtime.

  // 1. Initialize ROS 2
  rclcpp::init(argc, argv);

  // 2. Create a temporary node to read the component_type for logging.
  //    This is necessary so we can name the log file correctly before the main
  //    controller node, which also creates a logger, is constructed.
  auto param_fetcher_node = std::make_shared<rclcpp::Node>("gateway_param_fetcher");
  param_fetcher_node->declare_parameter<std::string>("identity.component_type", "UNKNOWN");
  std::string component_type = param_fetcher_node->get_parameter("identity.component_type").as_string();

  // 3. Set up the centralized logging system. This MUST be done before any
  //    other part of the gateway is constructed.
  gateway::setup_logging(component_type);

  // 4. Install signal handlers to flush logs on crash
  install_abort_handler();

  try {
    // 5. Create the main GatewayController node and initialize it properly
    auto controller_node = std::make_shared<gateway::GatewayController>(rclcpp::NodeOptions());
    
    // 6. Initialize components that require shared_from_this()
    controller_node->initialize();
    
    // 7. Spin the node
    rclcpp::spin(controller_node);
  } catch (const std::exception& e) {
    // Force log flush before re-throwing
    CppLogging::Config::Shutdown();
    throw;
  }

  // 8. Shut down ROS 2 and logging
  rclcpp::shutdown();
  CppLogging::Config::Shutdown();
  
  return 0;
}