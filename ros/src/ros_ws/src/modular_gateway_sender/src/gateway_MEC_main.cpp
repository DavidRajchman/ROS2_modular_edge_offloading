#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

// Handlers
#include "modular_gateway_sender/handlers/string_handler.hpp"
#include "modular_gateway_sender/handlers/laserscan_handler.hpp"
#include "modular_gateway_sender/handlers/string_test_input_handler.hpp"
#include "modular_gateway_sender/handlers/string_test_result_handler.hpp"


//This gateway is run in a MEC enviroment, it listens to data input and sends back the results (in terms of network connections)

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  
  // Create NodeOptions to override parameters
  rclcpp::NodeOptions node_options;
  std::vector<rclcpp::Parameter> initial_parameters;

  // Override parameters here
  initial_parameters.push_back(rclcpp::Parameter("server_port", 16001)); // Example: Override server_port
  initial_parameters.push_back(rclcpp::Parameter("id_group", 12));
  initial_parameters.push_back(rclcpp::Parameter("identifier_in_group", 1));
  // Add any other parameters you want to override

  node_options.parameter_overrides(initial_parameters);

  auto gateway = std::make_shared<gateway::RosGateway>("gateway_MEC_tcp_server", gateway::TransportMode::SERVER, node_options);

  // Configure string test input handler - as a publisher - receives testing inputs for processing
  auto string_test_input_handler = std::make_shared<gateway::StringTestInputHandler>(gateway.get());
  gateway::MessageHandlerBase::configure_handler_mode(string_test_input_handler, gateway::HandlerMode::PUBLISHER_ONLY);
  gateway->register_handler(string_test_input_handler);

  // Configure string test result handler - as a subciber - sends testing results back to the VHC
  auto string_test_result_handler = std::make_shared<gateway::StringTestResultHandler>(gateway.get());
  gateway::MessageHandlerBase::configure_handler_mode(string_test_result_handler, gateway::HandlerMode::SUBSCRIBER_ONLY);
  gateway->register_handler(string_test_result_handler);
  
  rclcpp::spin(gateway);
  rclcpp::shutdown();
  return 0;
}