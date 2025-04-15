/**
 * @file ros_gateway.hpp
 * @brief Defines the base RosGateway class for the modular gateway system.
 * 
 * This file contains the base class for all gateway modules in the system.
 * The RosGateway class connects ROS 2 nodes to network transport protocols,
 * handling message formatting, header creation, and transport management.
 * Gateway modules inherit from this class to create type-specific gateways.
 */

#ifndef ROS_GATEWAY_HPP
#define ROS_GATEWAY_HPP

#include <memory>
#include "rclcpp/rclcpp.hpp"
#include "transport_base.hpp"
#include "message_header.hpp"

// Version defines
#ifndef VERSION_MAJOR
#define VERSION_MAJOR 0
#endif

#ifndef VERSION_MINOR
#define VERSION_MINOR 2
#endif

#ifndef VERSION_PATCH
#define VERSION_PATCH 0
#endif

// Macro magic to convert defines to string
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

// Versioned logging macros
#define LOG_INFO(logger, format, ...) \
    RCLCPP_INFO(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

#define LOG_WARN(logger, format, ...) \
    RCLCPP_WARN(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

#define LOG_ERROR(logger, format, ...) \
    RCLCPP_ERROR(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

#define LOG_DEBUG(logger, format, ...) \
    RCLCPP_DEBUG(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

namespace gateway {

  /**
   * @brief Base class for all gateway modules in the system.
   * 
   * RosGateway serves as the foundation for all gateway modules by providing:
   * - Transport initialization and management
   * - Common parameter handling
   * - Message transmission with proper headers
   * - Connection management and error handling
   * 
   * Derived classes implement specific message type handling by subscribing
   * to ROS topics and calling the send_message methods with appropriate data.
   */
  class RosGateway : public rclcpp::Node {
  public:
    /**
     * @brief Constructs a RosGateway with the specified node name.
     * 
     * Initializes common parameters, sets up the shutdown handler,
     * and initializes the appropriate transport based on parameters.
     * 
     * @param node_name The name for this ROS node
     */
    RosGateway(const std::string& node_name);
    
    /**
     * @brief Virtual destructor ensures proper cleanup of derived classes.
     * 
     * Disconnects any active transport before destruction.
     */
    virtual ~RosGateway();
    
  protected:
    /**
     * @brief Sends a message with appropriate header using default flags.
     * 
     * Creates a header with the specified topic and type, then sends
     * both the header and data through the configured transport.
     * 
     * @param topic Topic name for the message
     * @param type Message type identifier
     * @param data Pointer to the message data
     * @param size Size of the message data in bytes
     * @return bool True if the message was sent successfully
     */
    bool send_message(const std::string& topic, MessageType type, const void* data, size_t size);
  
    /**
     * @brief Sends a message with custom header options.
     * 
     * Creates a header with the specified topic, type, and options,
     * then sends both the header and data through the configured transport.
     * 
     * @param topic Topic name for the message
     * @param type Message type identifier
     * @param data Pointer to the message data
     * @param size Size of the message data in bytes
     * @param options Custom message options for controlling header flags
     * @return bool True if the message was sent successfully
     */
    bool send_message(const std::string& topic, MessageType type,
      const void* data, size_t size,
      const MessageOptions& options);
    
    /**
     * @brief Initializes the transport based on parameters.
     * 
     * Reads the "transport_type", "server_host", "server_port", and
     * "max_retries" parameters and creates the appropriate transport
     * implementation.
     */
    void init_transport();
    
    /**
     * @brief The active transport implementation instance.
     * 
     * This member holds the concrete transport (TCP, UDP, etc.) that
     * handles the actual network communication.
     */
    std::unique_ptr<TransportBase> transport_;
  };
  
} // namespace gateway
  
#endif // ROS_GATEWAY_HPP