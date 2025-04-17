#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

RosGateway::RosGateway(const std::string& node_name)
  : Node(node_name)
{
  // Initialize parameters with defaults
  declare_parameter("transport_type", "tcp");
  declare_parameter("server_host", "100.98.247.80");
  declare_parameter("server_port", 8888);
  declare_parameter("max_retries", 3);
  
  // Set up shutdown handler
  rclcpp::on_shutdown([this]() { 
    if (transport_) {
      transport_->disconnect();
    }
  });
  
  // Initialize transport
  init_transport();
}

RosGateway::~RosGateway()
{
  if (transport_) {
    transport_->disconnect();
  }
}

void RosGateway::init_transport()
{
  std::string transport_type = get_parameter("transport_type").as_string();
  std::string server_host = get_parameter("server_host").as_string();
  int server_port = get_parameter("server_port").as_int();
  int max_retries = get_parameter("max_retries").as_int();
  
  if (transport_type == "tcp") {
    transport_ = std::make_unique<TcpTransport>(server_host, server_port, max_retries);
    LOG_INFO(get_logger(), "Initializing TCP transport to %s:%d", 
             server_host.c_str(), server_port);
  } 
  else if (transport_type == "udp") {
    // Future: create UDP transport
    LOG_ERROR(get_logger(), "UDP transport not yet implemented");
  }
  else {
    LOG_ERROR(get_logger(), "Unknown transport type: %s", transport_type.c_str());
  }
}



bool RosGateway::send_message(const std::string& topic, MessageType type, 
  const void* data, size_t size,
  const MessageOptions& options)
{
  if (!transport_) {
    LOG_ERROR(get_logger(), "No transport initialized");
    return false;
  }

  if (!transport_->is_connected()) {
    LOG_WARN(get_logger(), "Transport not connected, trying to connect...");
    if (!transport_->connect()) {
      LOG_ERROR(get_logger(), "Failed to connect transport");
      return false;
    }
  }

  // Create header using the options
  std::vector<uint8_t> header = create_header(topic, type, size, options);

  // Send header
  if (!transport_->send_data(header.data(), header.size())) {
    LOG_ERROR(get_logger(), "Failed to send header");
    return false;
  }

  // Send data
  if (!transport_->send_data(data, size)) {
    LOG_ERROR(get_logger(), "Failed to send data");
    return false;
  }

  LOG_INFO(get_logger(), "Sent %zu bytes to topic %s", size, topic.c_str());
  return true;
}

bool RosGateway::register_handler(std::shared_ptr<MessageHandlerBase> handler)
{
  if (!handler) {
    LOG_ERROR(get_logger(), "Cannot register null handler");
    return false;
  }
  
  const std::string& name = handler->get_name();
  if (handlers_.find(name) != handlers_.end()) {
    LOG_WARN(get_logger(), "Handler '%s' already registered", name.c_str());
    return false;
  }
  
  handlers_[name] = handler;
  handler->initialize();
  handler->enable();
  LOG_INFO(get_logger(), "Registered handler: %s", name.c_str());
  return true;
}

bool RosGateway::unregister_handler(const std::string& handler_name)
{
  auto it = handlers_.find(handler_name);
  if (it == handlers_.end()) {
    LOG_WARN(get_logger(), "Handler '%s' not found", handler_name.c_str());
    return false;
  }
  
  it->second->shutdown();
  handlers_.erase(it);
  LOG_INFO(get_logger(), "Unregistered handler: %s", handler_name.c_str());
  return true;
}

void RosGateway::enable_handler(const std::string& handler_name)
{
  auto it = handlers_.find(handler_name);
  if (it == handlers_.end()) {
    LOG_WARN(get_logger(), "Handler '%s' not found", handler_name.c_str());
    return;
  }
  
  it->second->enable();
  LOG_INFO(get_logger(), "Enabled handler: %s", handler_name.c_str());
}

void RosGateway::disable_handler(const std::string& handler_name)
{
  auto it = handlers_.find(handler_name);
  if (it == handlers_.end()) {
    LOG_WARN(get_logger(), "Handler '%s' not found", handler_name.c_str());
    return;
  }
  
  it->second->disable();
  LOG_INFO(get_logger(), "Disabled handler: %s", handler_name.c_str());
}

} // namespace gateway

