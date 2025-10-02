#include "modular_gateway_sender/ros_gateway.hpp"

namespace gateway {

// Constructor: Initialize data plane gateway with transport and pre-allocate buffers
// Manages binary message framing for ROS message transmission over TCP
RosGateway::RosGateway(
    rclcpp::Node::SharedPtr node,
    std::unique_ptr<TransportBase> transport)
  : node_(node),
    transport_(std::move(transport)),
    logger_(CppLogging::Logger("gateway"))
{
    logger_.Info("ros_gateway.cpp: RosGateway (Data Plane) constructed.");

    // Pre-allocate buffers to minimize runtime allocations and improve performance
    header_buffer_.reserve(256);          // Binary message header
    topic_buffer_.reserve(256);           // ROS topic name string
    data_buffer_.reserve(1024 * 1024);    // Message payload (1MB default)
}

// Destructor: Clean shutdown of receiver thread and transport connection
RosGateway::~RosGateway()
{
  logger_.Info("ros_gateway.cpp: RosGateway destructor called");
  stop_receiver();  // Stop message processing thread

  if (transport_) {
    transport_->disconnect();  // Close TCP connection
  }
}

// Send ROS message over data plane with binary framing
// Frame format: [Magic|Flags|Type|GroupID|IdInGroup|PayloadSize|TopicLen|Topic|Payload]
// Called by message handlers when ROS messages need to be transmitted
bool RosGateway::send_message(const std::string& topic, MessageType type, 
  const void* data, size_t size,
  const MessageOptions& options)
{
  if (!transport_) {
    logger_.Error("ros_gateway.cpp: send_message failed - no transport initialized");
    return false;
  }

  if (!transport_->is_connected()) {
    logger_.Warn("ros_gateway.cpp: send_message called but transport is not connected.");
    return false;
  }

  // Create binary header with message metadata
  logger_.Debug("ros_gateway.cpp: Creating header for topic '{}', size {} bytes", topic, size);
  create_header_in_buffer(header_buffer_, topic, type, id_group_, identifier_in_group_, size, options);

  // Copy header and payload to transport buffers
  std::vector<uint8_t> header(header_buffer_.begin(), header_buffer_.end());
  std::vector<uint8_t> payload(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);

  // Queue header for async transmission
  auto header_result = transport_->async_send_data(std::move(header));
  if (header_result != TransportAsyncSendResult::SUCCESS) {
    logger_.Error("ros_gateway.cpp: Failed to enqueue header for topic '{}', error code {}", topic, static_cast<int>(header_result));
    return false;
  }

  // Queue payload for async transmission
  auto payload_result = transport_->async_send_data(std::move(payload));
  if (payload_result != TransportAsyncSendResult::SUCCESS) {
    logger_.Error("ros_gateway.cpp: Failed to enqueue payload for topic '{}', error code {}", topic, static_cast<int>(payload_result));
    return false;
  }

  logger_.Debug("ros_gateway.cpp: Successfully enqueued {} bytes to topic '{}'", size, topic);
  return true;
}

// Start background thread to receive and process incoming binary messages
// Called by controller after DP_CONNECTION_CONFIRMED received from Bridge
void RosGateway::start_receiver() {
    if (receiver_running_) {
        logger_.Warn("ros_gateway.cpp: Receiver thread already running");
        return;
    }
    
    if (!transport_) {
        logger_.Error("ros_gateway.cpp: Cannot start receiver - no transport initialized");
        return;
    }

    // Launch dedicated thread for continuous message reception
    receiver_running_ = true;
    receiver_thread_ = std::thread(&RosGateway::receiver_thread_func, this);
    logger_.Info("ros_gateway.cpp: Started message receiver thread");
}

// Stop receiver thread gracefully
// Called during shutdown or when connection is lost
void RosGateway::stop_receiver() {
    if (!receiver_running_) {
        return;
    }
    
    receiver_running_ = false;  // Signal thread to exit
    if (receiver_thread_.joinable()) {
        logger_.Debug("ros_gateway.cpp: Waiting for receiver thread to join");
        receiver_thread_.join();  // Block until thread exits
    }
    logger_.Info("ros_gateway.cpp: Stopped message receiver thread");
}

// Background thread: continuously polls for incoming messages and processes them
// Runs until receiver_running_ flag is cleared
void RosGateway::receiver_thread_func() {
  int receiver_idle_poll_sleep_us = 100;  // Sleep duration when no data available

  while (receiver_running_) {
      bool data_ready = false;
      
      // Check if data available on transport with timeout
      if (transport_ && transport_->is_connected()) {
          data_ready = transport_->data_available(100);
      }
      
      if (data_ready) {
          // Parse binary frame and route to appropriate handler
          if (!receive_and_process_message()) {
              logger_.Warn("ros_gateway.cpp: Failed to process message or peer disconnected.");
              // Transport handles disconnect detection, continue polling
          }
      } else {
          // No data ready - sleep briefly to avoid busy waiting
          if (!receiver_running_) break;
          std::this_thread::sleep_for(std::chrono::microseconds(receiver_idle_poll_sleep_us));
      }
  }
}

// Parse binary message frame and dispatch to registered handlers
// Frame: [2:Magic|1:Flags|1:Type|1:GroupID|1:IdInGroup|4:PayloadSize|1:TopicLen|N:Topic|N:Payload]
// Returns false on parse error or connection failure
bool RosGateway::receive_and_process_message() {
    constexpr size_t HEADER_MIN_SIZE = 11;  // Minimum header before topic name
    uint8_t header_start[HEADER_MIN_SIZE];
    
    // Read fixed-size header portion
    logger_.Debug("ros_gateway.cpp: Receiving message header ({} bytes)", HEADER_MIN_SIZE);
    if (!transport_->receive_exact(header_start, HEADER_MIN_SIZE)) {
        logger_.Error("ros_gateway.cpp: Failed to receive header start");
        return false;
    }
    
    // Validate magic number (0xA5C3) to detect frame boundary
    uint16_t magic = (header_start[0] << 8) | header_start[1];
    if (magic != HEADER_MAGIC) {
        logger_.Error("ros_gateway.cpp: Invalid header magic: 0x{:04X} (expected 0x{:04X})", 
                      magic, HEADER_MAGIC);
        return false;
    }
    
    // Extract header fields (big-endian byte order)
    uint8_t flags = header_start[2];
    MessageType type = static_cast<MessageType>(header_start[3]);
    uint8_t received_id_group = header_start[4];
    uint8_t received_identifier_in_group = header_start[5];
    uint32_t data_size = (static_cast<uint32_t>(header_start[6]) << 24) | 
                         (static_cast<uint32_t>(header_start[7]) << 16) | 
                         (static_cast<uint32_t>(header_start[8]) << 8)  | 
                         static_cast<uint32_t>(header_start[9]);
    uint8_t topic_len = header_start[10];
    
    // Read variable-length topic name
    logger_.Debug("ros_gateway.cpp: Resizing topic buffer ({} bytes)", topic_len);
    try {
        topic_buffer_.resize(topic_len);
    } catch (const std::bad_alloc& e) {
        logger_.Error("ros_gateway.cpp: Failed to resize topic buffer: {}", e.what());
        return false;
    }
    
    if (!transport_->receive_exact(topic_buffer_.data(), topic_len)) {
        logger_.Error("ros_gateway.cpp: Failed to receive topic name");
        return false;
    }
    
    std::string topic(topic_buffer_.data(), topic_len);
    MessageOptions options(flags);  // Parse flags byte
    
    logger_.Debug("ros_gateway.cpp: Received header for '{}', type={}, from {}:{}, size={} bytes",
                  topic, static_cast<int>(type), received_id_group, received_identifier_in_group, data_size);
    
    // Read message payload
    logger_.Debug("ros_gateway.cpp: Resizing message data buffer ({} bytes)", data_size);
    try {
        data_buffer_.resize(data_size);
    } catch (const std::bad_alloc& e) {
        logger_.Error("ros_gateway.cpp: Failed to resize data buffer: {}", e.what());
        return false;
    }
    
    logger_.Debug("ros_gateway.cpp: Receiving message payload ({} bytes)", data_size);
    if (!transport_->receive_exact(data_buffer_.data(), data_size)) {
        logger_.Error("ros_gateway.cpp: Failed to receive message data for topic '{}'", topic);
        return false;
    }
    
    // Dispatch to appropriate handler based on MessageType
    bool processed = false;
    logger_.Debug("ros_gateway.cpp: Processing message through {} handlers", handlers_.size());
    
    for (const auto& handler_pair : handlers_) {
        auto& handler = handler_pair.second;
        
        // Check if handler can process this message type and is configured as publisher
        if (handler->is_ros_publisher_enabled() && handler->can_process_message_type(type)) {
            if (handler->process_and_publish_received_msg(topic, type, data_buffer_.data(), data_size, options)) {
                processed = true;  // Handler successfully published to ROS topic
            }
        }
    }
    
    if (!processed) {
        logger_.Warn("ros_gateway.cpp: No handler processed message type {} for topic '{}'", 
                     static_cast<int>(type), topic);
    }
    
    return true;
}

// Register message handler for specific MessageType
// Handler bridges between binary data plane and ROS topics
// Called by controller when session is approved
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
  handler->initialize();  // Create ROS publishers/subscribers
  handler->enable();      // Start processing messages
  logger_.Info("ros_gateway.cpp: Registered handler: {}", name);
  return true;
}

// Unregister and shutdown handler
// Called during session teardown when offloading terminates
bool RosGateway::unregister_handler(const std::string& handler_name)
{
  auto it = handlers_.find(handler_name);
  if (it == handlers_.end()) {
    logger_.Warn("ros_gateway.cpp: Handler '{}' not found for unregistration", handler_name);
    return false;
  }
  
  it->second->shutdown();  // Destroy ROS publishers/subscribers
  handlers_.erase(it);
  logger_.Info("ros_gateway.cpp: Unregistered handler: {}", handler_name);
  return true;
}

// Enable handler to start processing messages
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

// Disable handler to pause message processing without unregistering
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

// Set component identity for outgoing message headers
// Identity format: group_id:id_in_group (e.g., 60:5 for VHC, 70:1 for MEC)
void RosGateway::set_identity(uint8_t id_group, uint8_t identifier_in_group) {
  id_group_ = id_group;
  identifier_in_group_ = identifier_in_group;
  logger_.Info("ros_gateway.cpp: Gateway ID explicitly set to Group: {}, Identifier: {}",
               id_group_, identifier_in_group_);
}

// Get underlying transport for connection management
TransportBase* RosGateway::get_transport()
{
    return transport_.get();
}

} // namespace gateway