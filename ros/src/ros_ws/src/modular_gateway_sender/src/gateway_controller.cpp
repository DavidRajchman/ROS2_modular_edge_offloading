#include "modular_gateway_sender/gateway_controller.hpp"
#include "modular_gateway_sender/transport/tcp_client_transport.hpp"
#include "modular_gateway_sender/transport/tcp_server_transport.hpp"

// Handlers
#include "modular_gateway_sender/handlers/string_handler.hpp"
#include "modular_gateway_sender/handlers/laserscan_handler.hpp"
#include "modular_gateway_sender/handlers/string_test_input_handler.hpp"
#include "modular_gateway_sender/handlers/string_test_result_handler.hpp"

namespace gateway {

GatewayController::GatewayController(const rclcpp::NodeOptions& options)
  : rclcpp::Node("gateway_controller", options),
    logger_(CppLogging::Logger("gateway"))
{
  logger_.Info("gateway_controller.cpp: Constructing GatewayController...");

  this->declare_parameter<std::string>("transport.type", "client");
  this->declare_parameter<std::string>("transport.host", "127.0.0.1");
  this->declare_parameter<int>("transport.port", 12345);
  this->declare_parameter<std::vector<std::string>>("handlers", {"string"});
  this->declare_parameter<int>("identity.group_id", 0);
  this->declare_parameter<int>("identity.identifier_in_group", 0);

  transport_type_ = this->get_parameter("transport.type").as_string();
  host_ = this->get_parameter("transport.host").as_string();
  port_ = this->get_parameter("transport.port").as_int();
  handlers_to_load_ = this->get_parameter("handlers").as_string_array();
  id_group_ = this->get_parameter("identity.group_id").as_int();
  identifier_in_group_ = this->get_parameter("identity.identifier_in_group").as_int();

  initialize_gateway();

  running_ = true;
  connection_thread_ = std::thread(&GatewayController::connection_management_thread_func, this);
  
  logger_.Info("gateway_controller.cpp: GatewayController constructed successfully.");
}

GatewayController::~GatewayController()
{
  logger_.Info("gateway_controller.cpp: Shutting down GatewayController...");
  running_ = false;
  if (connection_thread_.joinable()) {
    connection_thread_.join();
  }
  
  if (gateway_) {
    gateway_->stop_receiver();
  }
  
  if (transport_) {
    transport_->disconnect();
  }
  logger_.Info("gateway_controller.cpp: GatewayController shut down.");
}

void GatewayController::initialize_gateway()
{
  logger_.Info("gateway_controller.cpp: Initializing gateway transport: type={}, host={}, port={}",
               transport_type_, host_, port_);

  if (transport_type_ == "client") {
    transport_ = std::make_unique<TcpClientTransport>(host_, port_);
  } else if (transport_type_ == "server") {
    transport_ = std::make_unique<TcpServerTransport>(port_);
  } else {
    logger_.Error("gateway_controller.cpp: Invalid transport type specified: {}", transport_type_);
    throw std::runtime_error("Invalid transport type");
  }

  gateway_ = std::make_unique<RosGateway>(this->shared_from_this(), std::move(transport_));
  gateway_->set_identity(id_group_, identifier_in_group_);
  
  load_handlers();
}

void GatewayController::load_handlers()
{
  logger_.Info("gateway_controller.cpp: Loading specified message handlers...");
  for (const auto& handler_name : handlers_to_load_) {
    logger_.Info("gateway_controller.cpp: Attempting to load handler: {}", handler_name);
    if (handler_name == "string") {
      gateway_->register_handler(std::make_shared<StringHandler>(gateway_.get(), this->shared_from_this()));
    } else if (handler_name == "laserscan") {
      gateway_->register_handler(std::make_shared<LaserScanHandler>(gateway_.get(), this->shared_from_this()));
    } else if (handler_name == "string_test_input") {
      gateway_->register_handler(std::make_shared<StringTestInputHandler>(gateway_.get(), this->shared_from_this()));
    } else if (handler_name == "string_test_result") {
      gateway_->register_handler(std::make_shared<StringTestResultHandler>(gateway_.get(), this->shared_from_this()));
    } else {
      logger_.Warn("gateway_controller.cpp: Unknown handler specified in config: {}", handler_name);
    }
  }
}

void GatewayController::connection_management_thread_func()
{
  bool receiver_started = false;

  while (running_) {
    auto* transport = gateway_->get_transport();
    if (!transport) {
      logger_.Error("gateway_controller.cpp: Transport is null in connection thread.");
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    if (!transport->is_connected()) {
      receiver_started = false;
      gateway_->stop_receiver();
      logger_.Info("gateway_controller.cpp: Transport not connected. Attempting to connect...");
      
      if (transport->connect()) {
        if (transport_type_ == "server") {
          logger_.Info("gateway_controller.cpp: Server listening. Waiting for client to accept...");
          if (!transport->accept_connection()) {
             // Failed to accept, will retry in the loop
             std::this_thread::sleep_for(std::chrono::milliseconds(500));
             continue;
          }
        }
        logger_.Info("gateway_controller.cpp: Connection established.");
      } else {
        logger_.Warn("gateway_controller.cpp: Connection attempt failed. Retrying...");
      }
    }

    if (transport->is_connected() && !receiver_started) {
      logger_.Info("gateway_controller.cpp: Connection is active, starting receiver thread.");
      gateway_->start_receiver();
      receiver_started = true;
    }
    
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

} // namespace gateway