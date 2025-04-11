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

// Base gateway class for common functionality
class RosGateway : public rclcpp::Node {
public:
  RosGateway(const std::string& node_name);
  virtual ~RosGateway();
  
protected:
  // Send message with appropriate header
  bool send_message(const std::string& topic, MessageType type, const void* data, size_t size);
  
  // Initialize transport based on parameters
  void init_transport();
  
  // Transport instance
  std::unique_ptr<TransportBase> transport_;
};

} // namespace gateway

#endif // ROS_GATEWAY_HPP