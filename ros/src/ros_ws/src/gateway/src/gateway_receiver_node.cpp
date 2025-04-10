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
// use by calling LOG_INFO(get_logger(), "example message") instead of RLCCP_INFO(....)

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

class TcpToRosGateway : public rclcpp::Node
{
public:
  TcpToRosGateway()
  : Node("ros_tcp_receiver"), server_socket_fd_(-1), client_socket_fd_(-1), connection_active_(false)
  {
    // Initialize parameters with defaults
    declare_parameter("tcp_listen_port", 8888);
    declare_parameter("tcp_listen_addr", "0.0.0.0");
    declare_parameter("ros_topic", "received_topic");
    
    // Get parameters
    listen_port_ = get_parameter("tcp_listen_port").as_int();
    listen_addr_ = get_parameter("tcp_listen_addr").as_string();
    std::string ros_topic = get_parameter("ros_topic").as_string();
    
    // Create ROS publisher
    publisher_ = create_publisher<std_msgs::msg::String>(ros_topic, 10);
    
    LOG_INFO(get_logger(), "Starting TCP receiver on %s:%d", 
                listen_addr_.c_str(), listen_port_);
    
    // Setup TCP server socket
    if (setup_server_socket()) {
      // Create a thread for accepting connections and receiving data
      receiver_thread_ = std::thread(&TcpToRosGateway::receiver_loop, this);
    }
    
    // Set up shutdown handler
    rclcpp::on_shutdown([this]() { close_connections(); });
  }

  ~TcpToRosGateway()
  {
    close_connections();
    if (receiver_thread_.joinable()) {
      receiver_thread_.join();
    }
  }

private:
  bool setup_server_socket() {
    // Create socket
    server_socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd_ < 0) {
      LOG_ERROR(get_logger(), "Failed to create socket: %s", strerror(errno));
      return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      LOG_ERROR(get_logger(), "Failed to set socket options: %s", strerror(errno));
      close(server_socket_fd_);
      server_socket_fd_ = -1;
      return false;
    }
    
    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(listen_port_);
    
    if (inet_pton(AF_INET, listen_addr_.c_str(), &server_addr.sin_addr) <= 0) {
      LOG_ERROR(get_logger(), "Invalid address: %s", strerror(errno));
      close(server_socket_fd_);
      server_socket_fd_ = -1;
      return false;
    }
    
    // Bind socket to address and port
    if (bind(server_socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      LOG_ERROR(get_logger(), "Bind failed: %s", strerror(errno));
      close(server_socket_fd_);
      server_socket_fd_ = -1;
      return false;
    }
    
    // Listen for connections
    if (listen(server_socket_fd_, 1) < 0) {
      LOG_ERROR(get_logger(), "Listen failed: %s", strerror(errno));
      close(server_socket_fd_);
      server_socket_fd_ = -1;
      return false;
    }
    
    LOG_INFO(get_logger(), "TCP server socket ready, waiting for connections");
    return true;
  }
  
  void receiver_loop() {
    while (rclcpp::ok()) {
      // If no active connection, wait for a new one
      if (!connection_active_) {
        accept_connection();
      }
      
      // If we have an active connection, receive data
      if (connection_active_) {
        receive_data();
      }
      
      // Small sleep to prevent busy waiting
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  
  void accept_connection() {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // Set server socket to non-blocking for accept
    int flags = fcntl(server_socket_fd_, F_GETFL, 0);
    fcntl(server_socket_fd_, F_SETFL, flags | O_NONBLOCK);
    
    // Try to accept a connection
    client_socket_fd_ = accept(server_socket_fd_, (struct sockaddr*)&client_addr, &addr_len);
    
    if (client_socket_fd_ < 0) {
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        LOG_ERROR(get_logger(), "Accept failed: %s", strerror(errno));
      }
      return;
    }
    
    // Get client IP as string
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    
    LOG_INFO(get_logger(), "Accepted connection from %s:%d", 
                client_ip, ntohs(client_addr.sin_port));
    
    // Set client socket to non-blocking mode for reading
    flags = fcntl(client_socket_fd_, F_GETFL, 0);
    fcntl(client_socket_fd_, F_SETFL, flags | O_NONBLOCK);
    
    connection_active_ = true;
  }
  
  void receive_data() {
    if (client_socket_fd_ < 0) {
      connection_active_ = false;
      return;
    }
    
    struct pollfd pfd;
    pfd.fd = client_socket_fd_;
    pfd.events = POLLIN;
    
    // Wait for data with a short timeout
    int poll_result = poll(&pfd, 1, 100);
    
    if (poll_result < 0) {
      LOG_ERROR(get_logger(), "Poll failed: %s", strerror(errno));
      close_client_connection();
      return;
    } else if (poll_result == 0) {
      // Timeout, no data available
      return;
    }
    
    // Data is available, read it
    char buffer[4096];
    ssize_t bytes_read = recv(client_socket_fd_, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read < 0) {
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        LOG_ERROR(get_logger(), "Receive failed: %s", strerror(errno));
        close_client_connection();
      }
      return;
    } else if (bytes_read == 0) {
      LOG_INFO(get_logger(), "Client disconnected");
      close_client_connection();
      return;
    }
    
    // Null-terminate the received data
    buffer[bytes_read] = '\0';
    std::string received_data(buffer);
    
    LOG_INFO(get_logger(), "Received from client: %s", received_data.c_str());
    
    // Parse message format: [length]:[data]\n
    size_t colon_pos = received_data.find(':');
    if (colon_pos != std::string::npos) {
      std::string message = received_data.substr(colon_pos + 1);
      
      // Remove trailing newline if present
      if (!message.empty() && message.back() == '\n') {
        message.pop_back();
      }
      
      // Create and publish ROS message
      auto msg = std::make_unique<std_msgs::msg::String>();
      msg->data = message;
      publisher_->publish(std::move(msg));
      
      // ACK removed - no response sent
    }
  }
  
  void close_client_connection() {
    if (client_socket_fd_ >= 0) {
      close(client_socket_fd_);
      client_socket_fd_ = -1;
    }
    connection_active_ = false;
  }
  
  void close_connections() {
    close_client_connection();
    
    if (server_socket_fd_ >= 0) {
      close(server_socket_fd_);
      server_socket_fd_ = -1;
    }
    
    LOG_INFO(get_logger(), "All TCP connections closed");
  }

  // ROS publisher
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
  
  // TCP variables
  int server_socket_fd_;
  int client_socket_fd_;
  std::string listen_addr_;
  int listen_port_;
  bool connection_active_;
  
  // Thread for accepting connections and receiving data
  std::thread receiver_thread_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<TcpToRosGateway>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}