#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

void ConfigureLogger()
{
    // Use AsyncWaitFreeProcessor for optimal multithreaded performance
    auto sink = std::make_shared<CppLogging::AsyncWaitFreeProcessor>(
        std::make_shared<CppLogging::BinaryLayout>(),
        true,    // auto_start
        8192,    // capacity (power of 2)
        false    // don't discard - block if buffer full (prevents message loss)
    );
    
    sink->appenders().push_back(
        std::make_shared<CppLogging::FileAppender>("mec_binary.log"));
    
    CppLogging::Config::ConfigLogger("gateway", sink);
}

namespace gateway {

RosGateway::RosGateway(
    const std::string& node_name,
    TransportMode transport_mode,
    const rclcpp::NodeOptions& options)
  : Node(node_name, options),
    transport_mode_(transport_mode)
{
    // Initialize parameters with defaults for client mode
    this->declare_parameter("transport_type", "tcp");
    this->declare_parameter("server_host", "127.0.0.1");
    this->declare_parameter("server_port", 12888);
    this->declare_parameter("max_retries", 3);
    this->declare_parameter("auto_start_receiver", true);
    this->declare_parameter("wait_for_connection", true);
    this->declare_parameter("connection_timeout_ms", 5000);
    this->declare_parameter("receiver_sleep_time_us", 100);
    this->declare_parameter("id_group", 0);
    this->declare_parameter("identifier_in_group", 0);

    ConfigureLogger();
    CppLogging::Config::Startup();
    
    logger_ = CppLogging::Logger("gateway");
    logger_.Info("ros_gateway.cpp: Gateway constructor starting for node '{}'", node_name);

    id_group_ = static_cast<uint8_t>(this->get_parameter("id_group").as_int());
    identifier_in_group_ = static_cast<uint8_t>(this->get_parameter("identifier_in_group").as_int());
  
    logger_.Info("ros_gateway.cpp: Gateway configured with ID Group: {}, Identifier: {}", 
                 id_group_, identifier_in_group_);

    // Set up shutdown handler
    rclcpp::on_shutdown([this]() { 
      logger_.Info("ros_gateway.cpp: Shutdown signal received, stopping receiver");
      stop_receiver();

      if (transport_) {
        transport_->disconnect();
      }
    });

    // #1 LATENCY POINT: Buffer allocation
    logger_.Debug("ros_gateway.cpp: Pre-allocating header buffer");
    header_buffer_.reserve(256);
    
    // Initialize transport
    init_transport();
    
    // Automatically start receiver if parameter is set
    if (get_parameter("auto_start_receiver").as_bool()) {
      bool wait = get_parameter("wait_for_connection").as_bool();
      int timeout = get_parameter("connection_timeout_ms").as_int();
      logger_.Info("ros_gateway.cpp: Auto-starting receiver (wait={}, timeout={}ms)", wait, timeout);
      start_receiver(wait, timeout);
    }
}

RosGateway::~RosGateway()
{
  logger_.Info("ros_gateway.cpp: Gateway destructor called");
  stop_receiver();

  if (transport_) {
    transport_->disconnect();
  }
}

void RosGateway::init_transport()
{
  // #18 LATENCY POINT: Parameter access
  logger_.Debug("ros_gateway.cpp: Loading transport configuration parameters");
  std::string transport_type = get_parameter("transport_type").as_string();
  
  if (transport_type == "tcp") {
    if (transport_mode_ == TransportMode::CLIENT) {
      std::string server_host = get_parameter("server_host").as_string();
      int server_port = get_parameter("server_port").as_int();
      int max_retries = get_parameter("max_retries").as_int();
      
      transport_ = std::make_unique<TcpClientTransport>(server_host, server_port, max_retries);
      logger_.Info("ros_gateway.cpp: Initialized TCP client transport to {}:{}", 
                   server_host, server_port);
    }
    else { // SERVER mode
      int server_port = get_parameter("server_port").as_int();
      int max_connections = 1;
      
      transport_ = std::make_unique<TcpServerTransport>(server_port, max_connections);
      logger_.Info("ros_gateway.cpp: Initialized TCP server transport on port {}", server_port);
    }
  } 
  else if (transport_type == "udp") {
    logger_.Error("ros_gateway.cpp: UDP transport not yet implemented");
  }
  else {
    logger_.Error("ros_gateway.cpp: Unknown transport type: {}", transport_type);
  }
}

bool RosGateway::send_message(const std::string& topic, MessageType type, 
  const void* data, size_t size,
  const MessageOptions& options)
{
  if (!transport_) {
    logger_.Error("ros_gateway.cpp: send_message failed - no transport initialized");
    return false;
  }

  // #20-21 LATENCY POINT: Connection status check and establishment
  if (!transport_->is_connected()) {
    logger_.Warn("ros_gateway.cpp: Transport not connected, attempting connection for topic '{}'", topic);
    if (!transport_->connect()) {
      logger_.Error("ros_gateway.cpp: Failed to connect transport for topic '{}'", topic);
      return false;
    }
  }

  // #2 LATENCY POINT: Header buffer manipulation
  logger_.Debug("ros_gateway.cpp: Creating header for topic '{}', size {} bytes", topic, size);
  create_header_in_buffer(header_buffer_, topic, type, id_group_, identifier_in_group_, size, options);

  // #12 LATENCY POINT: Critical section lock entry
  logger_.Debug("ros_gateway.cpp: Acquiring transport lock for send operation");
  {
    std::lock_guard<std::mutex> lock(transport_access_mutex_);

    // #6 LATENCY POINT: Header network transmission
    logger_.Debug("ros_gateway.cpp: Sending header ({} bytes)", header_buffer_.size());
    if (!transport_->send_data(header_buffer_.data(), header_buffer_.size())) {
      logger_.Error("ros_gateway.cpp: Failed to send header for topic '{}'", topic);
      return false;
    }

    // #7 LATENCY POINT: Payload network transmission
    logger_.Debug("ros_gateway.cpp: Sending payload ({} bytes)", size);
    if (!transport_->send_data(data, size)) {
      logger_.Error("ros_gateway.cpp: Failed to send data for topic '{}'", topic);
      return false;
    }
  }
  // Critical section exit logged implicitly
  
  logger_.Debug("ros_gateway.cpp: Successfully sent {} bytes to topic '{}'", size, topic);
  return true;
}

bool RosGateway::start_receiver(bool wait_for_connection, int timeout_ms) {
    if (receiver_running_) {
        logger_.Warn("ros_gateway.cpp: Receiver thread already running");
        return false;
    }
    
    if (!transport_) {
        logger_.Error("ros_gateway.cpp: Cannot start receiver - no transport initialized");
        return false;
    }

    // Special handling based on transport mode
    if (transport_mode_ == TransportMode::CLIENT) {
        if (wait_for_connection && !transport_->is_connected()) {
            logger_.Info("ros_gateway.cpp: Waiting for connection (timeout={}ms)", timeout_ms);
            
            const int retry_interval_ms = 100;
            int time_waited = 0;
            
            // #20-22 LATENCY POINT: Connection retry loop
            while (!transport_->is_connected() && (timeout_ms <= 0 || time_waited < timeout_ms)) {
                logger_.Debug("ros_gateway.cpp: Connection attempt {}", time_waited / retry_interval_ms + 1);
                if (!transport_->connect()) {
                    logger_.Debug("ros_gateway.cpp: Connection failed, retrying in {}ms", retry_interval_ms);
                } else {
                    logger_.Info("ros_gateway.cpp: Transport connected after {}ms", time_waited);
                    break;
                }
                
                // #23 LATENCY POINT: Thread sleep
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_interval_ms));
                time_waited += retry_interval_ms;
            }
            if (!transport_->is_connected()) {
                logger_.Error("ros_gateway.cpp: Failed to connect transport after {}ms", time_waited);
                return false;
            }
        }
        else if (!transport_->is_connected()) {
            logger_.Error("ros_gateway.cpp: Cannot start receiver - transport not connected");
            return false;
        }
    } 
    else {
        // #20 LATENCY POINT: Server transport connection
        logger_.Debug("ros_gateway.cpp: Starting server transport");
        if (!transport_->connect()) {
            logger_.Error("ros_gateway.cpp: Failed to start server transport");
            return false;
        }
        logger_.Info("ros_gateway.cpp: Server transport listening for connections");
    }
    
    receiver_running_ = true;
    receiver_thread_ = std::thread(&RosGateway::receiver_thread_func, this);
    logger_.Info("ros_gateway.cpp: Started message receiver thread");
    return true;
}

void RosGateway::stop_receiver() {
    if (!receiver_running_) {
        return;
    }
    
    receiver_running_ = false;
    if (receiver_thread_.joinable()) {
        // #24 LATENCY POINT: Thread synchronization
        logger_.Debug("ros_gateway.cpp: Waiting for receiver thread to join");
        receiver_thread_.join();
    }
    logger_.Info("ros_gateway.cpp: Stopped message receiver thread");
}

void RosGateway::receiver_thread_func() {
  // #18 LATENCY POINT: Parameter access
  int sleep_time_us = get_parameter("receiver_sleep_time_us").as_int();

  while (receiver_running_) {
      bool data_ready = false;
      
      // #13 LATENCY POINT: First critical section lock
      {
          std::lock_guard<std::mutex> lock(transport_access_mutex_);
          if (transport_ && transport_->is_connected()) {
              // #11 LATENCY POINT: Network polling
              data_ready = transport_->data_available(10);
          }
      }
      
      // #14 LATENCY POINT: Second critical section lock (only if data exists)
      if (data_ready) {
          logger_.Debug("ros_gateway.cpp: Data available, acquiring lock for message processing");
          std::lock_guard<std::mutex> lock(transport_access_mutex_);
          if (!receive_and_process_message()) {
              logger_.Warn("ros_gateway.cpp: Message processing failed in receiver thread");
          }
      } else {
        // #23 LATENCY POINT: Thread sleep
        std::this_thread::sleep_for(std::chrono::microseconds(sleep_time_us));
      }
  }
}

bool RosGateway::receive_and_process_message() {
    constexpr size_t HEADER_MIN_SIZE = 11;
    uint8_t header_start[HEADER_MIN_SIZE];
    
    // #8 LATENCY POINT: Header network reception
    logger_.Debug("ros_gateway.cpp: Receiving message header ({} bytes)", HEADER_MIN_SIZE);
    if (!transport_->receive_exact(header_start, HEADER_MIN_SIZE)) {
        logger_.Error("ros_gateway.cpp: Failed to receive header start");
        return false;
    }
    
    // #25-26 LATENCY POINT: Magic number verification and data extraction
    uint16_t magic = (header_start[0] << 8) | header_start[1];
    if (magic != HEADER_MAGIC) {
        logger_.Error("ros_gateway.cpp: Invalid header magic: 0x{:04X} (expected 0x{:04X})", 
                      magic, HEADER_MAGIC);
        return false;
    }
    
    uint8_t flags = header_start[2];
    MessageType type = static_cast<MessageType>(header_start[3]);
    uint8_t received_id_group = header_start[4];
    uint8_t received_identifier_in_group = header_start[5];
    uint32_t data_size = (static_cast<uint32_t>(header_start[6]) << 24) | 
                         (static_cast<uint32_t>(header_start[7]) << 16) | 
                         (static_cast<uint32_t>(header_start[8]) << 8)  | 
                         static_cast<uint32_t>(header_start[9]);
    uint8_t topic_len = header_start[10];
    
    // #3 LATENCY POINT: Topic buffer allocation
    logger_.Debug("ros_gateway.cpp: Allocating topic buffer ({} bytes)", topic_len + 1);
    std::vector<char> topic_buf(topic_len + 1, '\0');
    
    // #9 LATENCY POINT: Topic name network reception
    if (!transport_->receive_exact(topic_buf.data(), topic_len)) {
        logger_.Error("ros_gateway.cpp: Failed to receive topic name");
        return false;
    }
    
    // #5 LATENCY POINT: String construction
    std::string topic(topic_buf.data(), topic_len);
    MessageOptions options(flags);
    
    logger_.Debug("ros_gateway.cpp: Received header for '{}', type={}, from {}:{}, size={} bytes",
                  topic, static_cast<int>(type), received_id_group, received_identifier_in_group, data_size);
    
    // #4 LATENCY POINT: Large data buffer allocation
    logger_.Debug("ros_gateway.cpp: Allocating message data buffer ({} bytes)", data_size);
    std::vector<uint8_t> data_buffer(data_size);
    
    // #10 LATENCY POINT: Payload network reception
    logger_.Debug("ros_gateway.cpp: Receiving message payload ({} bytes)", data_size);
    if (!transport_->receive_exact(data_buffer.data(), data_size)) {
        logger_.Error("ros_gateway.cpp: Failed to receive message data for topic '{}'", topic);
        return false;
    }
    
    // #15-16 LATENCY POINT: Handler processing loop
    bool processed = false;
    logger_.Debug("ros_gateway.cpp: Processing message through {} handlers", handlers_.size());
    
    for (const auto& handler_pair : handlers_) {
        auto& handler = handler_pair.second;
        
        if (handler->is_ros_publisher_enabled() && handler->can_process_message_type(type)) {
            logger_.Debug("ros_gateway.cpp: Handler '{}' processing message", handler->get_name());
            if (handler->process_and_publish_received_msg(
                    topic, type, data_buffer.data(), data_buffer.size(), options)) {
                logger_.Debug("ros_gateway.cpp: Message '{}' published to ROS by handler '{}'", 
                             topic, handler->get_name());
                processed = true;
            }
        }
    }
    
    if (!processed) {
        logger_.Warn("ros_gateway.cpp: No handler processed message type {} for topic '{}'", 
                     static_cast<int>(type), topic);
    }
    
    return true;
}

bool RosGateway::register_handler(std::shared_ptr<MessageHandlerBase> handler)
{
  if (!handler) {
    logger_.Error("ros_gateway.cpp: Cannot register null handler");
    return false;
  }
  
  const std::string& name = handler->get_name();
  if (handlers_.find(name) != handlers_.end()) {
    logger_.Warn("ros_gateway.cpp: Handler '{}' already registered", name);
    return false;
  }
  
  handlers_[name] = handler;
  handler->initialize();
  handler->enable();
  logger_.Info("ros_gateway.cpp: Registered handler: {}", name);
  return true;
}

bool RosGateway::unregister_handler(const std::string& handler_name)
{
  auto it = handlers_.find(handler_name);
  if (it == handlers_.end()) {
    logger_.Warn("ros_gateway.cpp: Handler '{}' not found for unregistration", handler_name);
    return false;
  }
  
  it->second->shutdown();
  handlers_.erase(it);
  logger_.Info("ros_gateway.cpp: Unregistered handler: {}", handler_name);
  return true;
}

void RosGateway::enable_handler(const std::string& handler_name)
{
  auto it = handlers_.find(handler_name);
  if (it == handlers_.end()) {
    logger_.Warn("ros_gateway.cpp: Handler '{}' not found for enabling", handler_name);
    return;
  }
  
  it->second->enable();
  logger_.Info("ros_gateway.cpp: Enabled handler: {}", handler_name);
}

void RosGateway::disable_handler(const std::string& handler_name)
{
  auto it = handlers_.find(handler_name);
  if (it == handlers_.end()) {
    logger_.Warn("ros_gateway.cpp: Handler '{}' not found for disabling", handler_name);
    return;
  }
  
  it->second->disable();
  logger_.Info("ros_gateway.cpp: Disabled handler: {}", handler_name);
}

void RosGateway::set_gateway_id(uint8_t id_group, uint8_t identifier_in_group) {
  id_group_ = id_group;
  identifier_in_group_ = identifier_in_group;
  logger_.Info("ros_gateway.cpp: Gateway ID explicitly set to Group: {}, Identifier: {}",
               id_group_, identifier_in_group_);
}

} // namespace gateway