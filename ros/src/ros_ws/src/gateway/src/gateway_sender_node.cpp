#include <memory>
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <chrono>
#include <thread>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"

// START OF VERSION MACRO -----------------------------------------------------------------------------------    
// use by calling LOG_INFO(get_logger(), "example message") instead of RCLCPP_INFO(....)

// Version defines - update these as needed or define them in a CMakeLists.txt
#ifndef VERSION_MAJOR
#define VERSION_MAJOR 0
#endif

#ifndef VERSION_MINOR
#define VERSION_MINOR 2
#endif

#ifndef VERSION_PATCH
#define VERSION_PATCH 0
#endif

// #region VERSION MACRO DEFINITIONS
// Macro magic to convert defines to string
#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)
#define VERSION_STRING STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)

// Versioned logging macros - use these instead of RCLCPP_* macros
#define LOG_INFO(logger, format, ...) \
    RCLCPP_INFO(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

#define LOG_WARN(logger, format, ...) \
    RCLCPP_WARN(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

#define LOG_ERROR(logger, format, ...) \
    RCLCPP_ERROR(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

#define LOG_DEBUG(logger, format, ...) \
    RCLCPP_DEBUG(logger, "[" VERSION_STRING "]: " format, ##__VA_ARGS__)

// #endregion VERSION MACRO DEFINITIONS

class RosToTcpGateway : public rclcpp::Node
{
public:
  RosToTcpGateway()
  : Node("ros_tcp_gateway"), socket_fd_(-1), connection_active_(false)
  {
    // Initialize parameters with defaults
    declare_parameter("tcp_server_host", "127.0.0.1");
    declare_parameter("tcp_server_port", 8888);
    declare_parameter("tcp_max_retries", 3);
    declare_parameter("ros_topic", "topic");
    
    // Get parameters
    server_host_ = get_parameter("tcp_server_host").as_string();
    server_port_ = get_parameter("tcp_server_port").as_int();
    max_retries_ = get_parameter("tcp_max_retries").as_int();
    std::string ros_topic = get_parameter("ros_topic").as_string();
    
    LOG_INFO(get_logger(), "Initializing TCP connection to %s:%d", 
                server_host_.c_str(), server_port_);
    
    // Setup TCP connection
    setup_tcp_connection();
    
    // Create ROS subscription
    subscription_ = create_subscription<std_msgs::msg::String>(
      ros_topic, 10,
      [this](std_msgs::msg::String::UniquePtr msg) {
        LOG_INFO(this->get_logger(), "Received from ROS: '%s'", msg->data.c_str());
        send_message(msg->data);
      }
    );
    
    // Set up shutdown handler
    rclcpp::on_shutdown([this]() { close_connection(); });
  }

  ~RosToTcpGateway()
  {
    close_connection();
  }

private:
  bool setup_tcp_connection() {
    // Close existing connection if any
    if (socket_fd_ >= 0) {
      close(socket_fd_);
      socket_fd_ = -1;
      connection_active_ = false;
    }
    
    try {
      // Create socket
      socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
      if (socket_fd_ < 0) {
        LOG_ERROR(get_logger(), "Failed to create socket: %s", strerror(errno));
        return false;
      }
      
      // Set up server address
      struct sockaddr_in server_addr;
      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(server_port_);
      
      if (inet_pton(AF_INET, server_host_.c_str(), &server_addr.sin_addr) <= 0) {
        LOG_ERROR(get_logger(), "Invalid address: %s", strerror(errno));
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
      }
      
      LOG_INFO(get_logger(), "Connecting to TCP server...");
      
      // Connect to server
      if (connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR(get_logger(), "Connection failed: %s", strerror(errno));
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
      }
      
      LOG_INFO(get_logger(), "Connected to TCP server successfully");
      connection_active_ = true;
      return true;
    }
    catch (const std::exception& ex) {
      LOG_ERROR(get_logger(), "TCP connection setup failed: %s", ex.what());
      if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
      }
      connection_active_ = false;
      return false;
    }
  }
  
  bool send_message(const std::string& message) {
    if (!connection_active_ || socket_fd_ < 0) {
      LOG_WARN(get_logger(), "TCP connection not active, trying to reconnect...");
      if (!setup_tcp_connection()) {
        LOG_ERROR(get_logger(), "Failed to reconnect to TCP server");
        return false;
      }
    }
    
    // Add a simple protocol: message length + message
    std::string framed_message = std::to_string(message.length()) + ":" + message + "\n";
    
    // Send the message with retries for connection issues
    for (int retry = 0; retry <= max_retries_; ++retry) {
      try {
        if (retry > 0) {
          LOG_WARN(get_logger(), "Retrying send (attempt %d of %d)...", 
                      retry, max_retries_);
        }
        
        // Send the message
        ssize_t bytes_sent = send(socket_fd_, framed_message.c_str(), framed_message.length(), 0);
        if (bytes_sent < 0) {
          if (errno == EWOULDBLOCK || errno == EAGAIN) {
            LOG_WARN(get_logger(), "Send would block, retrying...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
          } else {
            LOG_ERROR(get_logger(), "Send failed: %s", strerror(errno));
            connection_active_ = false;
            return false;
          }
        } else if (bytes_sent == 0) {
          LOG_ERROR(get_logger(), "Connection closed by peer");
          connection_active_ = false;
          return false;
        } else if (static_cast<size_t>(bytes_sent) < framed_message.length()) {
          LOG_WARN(get_logger(), "Partial send: %zd of %zu bytes", 
                      bytes_sent, framed_message.length());
          // Handle partial sends - try again
          continue;
        }
        
        // Message sent successfully
        LOG_INFO(get_logger(), "Sent to TCP server: %s", message.c_str());
        return true;
      }
      catch (const std::exception& ex) {
        LOG_ERROR(get_logger(), "Send failed with exception: %s", ex.what());
        connection_active_ = false;
      }
    }
    
    LOG_ERROR(get_logger(), "Failed to send message after %d retries", max_retries_);
    return false;
  }
  
  void close_connection() {
    if (socket_fd_ >= 0) {
      LOG_INFO(get_logger(), "Closing TCP connection...");
      
      // Send a graceful disconnect message if connection is still active
      if (connection_active_) {
        const char* bye_msg = "BYE\n";
        send(socket_fd_, bye_msg, strlen(bye_msg), 0);
      }
      
      close(socket_fd_);
      socket_fd_ = -1;
      connection_active_ = false;
      LOG_INFO(get_logger(), "TCP connection closed");
    }
  }

  // ROS subscription
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
  
  // TCP variables
  int socket_fd_;
  std::string server_host_;
  int server_port_;
  int max_retries_;
  bool connection_active_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<RosToTcpGateway>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}