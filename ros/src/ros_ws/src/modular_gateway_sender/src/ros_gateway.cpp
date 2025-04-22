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
  declare_parameter("auto_start_receiver", true);
  declare_parameter("wait_for_connection", true);
  declare_parameter("connection_timeout_ms", 5000);  // 5 seconds default timeout
  
  // Set up shutdown handler
  rclcpp::on_shutdown([this]() { 
    stop_receiver();

    if (transport_) {
      transport_->disconnect();
    }
  });
  
  // Initialize transport
  init_transport();
  
  // Automatically start receiver if parameter is set
  if (get_parameter("auto_start_receiver").as_bool()) {
    bool wait = get_parameter("wait_for_connection").as_bool();
    int timeout = get_parameter("connection_timeout_ms").as_int();
    start_receiver(wait, timeout);
  }
}

RosGateway::~RosGateway()
{
  // Stop receiver thread before disconnecting transport
  stop_receiver();

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
{
  std::lock_guard<std::mutex> lock(transport_access_mutex_); // Add mutex protection

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
}
  LOG_INFO(get_logger(), "Sent %zu bytes to topic %s", size, topic.c_str());
  return true;
}

bool RosGateway::start_receiver(bool wait_for_connection, int timeout_ms) {
    if (receiver_running_) {
        LOG_WARN(get_logger(), "Receiver thread already running");
        return false;
    }
    
    if (!transport_) {
        LOG_ERROR(get_logger(), "Cannot start receiver - no transport initialized");
        return false;
    }

    // Check if we need to wait for a connection
    if (wait_for_connection && !transport_->is_connected()) {
        LOG_INFO(get_logger(), "Transport not connected, waiting for connection...");
        
        const int retry_interval_ms = 100; // Check every 100ms
        int time_waited = 0;
        
        while (!transport_->is_connected() && (timeout_ms <= 0 || time_waited < timeout_ms)) {
            if (!transport_->connect()) {
                LOG_DEBUG(get_logger(), "Connection attempt failed, retrying...");
            } else {
                LOG_INFO(get_logger(), "Transport connected after waiting %d ms", time_waited);
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(retry_interval_ms));
            time_waited += retry_interval_ms;
        }
        
        if (!transport_->is_connected()) {
            LOG_ERROR(get_logger(), "Failed to connect transport after %d ms", time_waited);
            return false;
        }
    }
    else if (!transport_->is_connected()) {
        LOG_ERROR(get_logger(), "Cannot start receiver - transport not connected");
        return false;
    }
    
    receiver_running_ = true;
    receiver_thread_ = std::thread(&RosGateway::receiver_thread_func, this);
    LOG_INFO(get_logger(), "Started message receiver thread");
    return true;
}

void RosGateway::stop_receiver() {
    if (!receiver_running_) {
        return;
    }
    
    receiver_running_ = false;
    if (receiver_thread_.joinable()) {
        receiver_thread_.join();
    }
    LOG_INFO(get_logger(), "Stopped message receiver thread");
}

void RosGateway::receiver_thread_func() {
  while (receiver_running_) {
      bool data_ready = false;
      
      // FIRST CRITICAL SECTION: Only check for data (quick operation)
      {
          std::lock_guard<std::mutex> lock(transport_access_mutex_);
          if (transport_ && transport_->is_connected()) {
              data_ready = transport_->data_available(10);  // Just check, don't process
          }
      }
      
      // SECOND CRITICAL SECTION: Only if data exists (separate lock)
      if (data_ready) {
          std::lock_guard<std::mutex> lock(transport_access_mutex_);
          receive_and_process_message();  // Process with its own lock
      } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
  }
}

bool RosGateway::receive_and_process_message() {
    // Step 1: Read the first 9 bytes (magic, flags, type, size, topic_len)
    constexpr size_t HEADER_MIN_SIZE = 9;
    uint8_t header_start[HEADER_MIN_SIZE];
    
    if (!transport_->receive_exact(header_start, HEADER_MIN_SIZE)) {
        LOG_ERROR(get_logger(), "Failed to receive header start");
        return false;
    }
    
    // Step 2: Verify magic number
    uint16_t magic = (header_start[0] << 8) | header_start[1];
    if (magic != HEADER_MAGIC) {
        LOG_ERROR(get_logger(), "Invalid header magic: 0x%04X (expected 0x%04X)", magic, HEADER_MAGIC);
        return false;
    }
    
    // Step 3: Extract basic header information
    uint8_t flags = header_start[2];
    MessageType type = static_cast<MessageType>(header_start[3]);
    uint32_t data_size = (header_start[4] << 24) | (header_start[5] << 16) | 
                         (header_start[6] << 8) | header_start[7];
    uint8_t topic_len = header_start[8];
    
    // Step 4: Read the topic name
    std::vector<char> topic_buf(topic_len + 1, '\0');  // +1 for null terminator
    if (!transport_->receive_exact(topic_buf.data(), topic_len)) {
        LOG_ERROR(get_logger(), "Failed to receive topic name");
        return false;
    }
    
    std::string topic(topic_buf.data(), topic_len);
    MessageOptions options(flags);
    
    LOG_INFO(get_logger(), "Received message header for topic '%s', type %d, size %u bytes",
             topic.c_str(), static_cast<int>(type), data_size);
    
    // Step 5: Read the message payload
    std::vector<uint8_t> data_buffer(data_size);
    if (!transport_->receive_exact(data_buffer.data(), data_size)) {
        LOG_ERROR(get_logger(), "Failed to receive message data");
        return false;
    }
    
    LOG_INFO(get_logger(), "Successfully received %u bytes of message data", data_size);
    
    // Step 6: Forward the message to appropriate handlers
    // This would publish to ROS topics, call handler methods, etc.
    // For now, this is just a placeholder as you'll need to implement
    // the specific message handling logic
    
    LOG_INFO(get_logger(), "Message processed successfully");
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

