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

class RosToTcpGateway : public rclcpp::Node
{
public:
  RosToTcpGateway()
  : Node("ros_tcp_gateway"), socket_fd_(-1), connection_active_(false)
  {
    // Initialize parameters with defaults
    declare_parameter("tcp_server_host", "davidpraha.buru-palermo.ts.net");
    declare_parameter("tcp_server_port", 8888);
    declare_parameter("tcp_ack_timeout_ms", 2000);
    declare_parameter("tcp_max_retries", 3);
    declare_parameter("ros_topic", "topic");
    
    // Get parameters
    server_host_ = get_parameter("tcp_server_host").as_string();
    server_port_ = get_parameter("tcp_server_port").as_int();
    ack_timeout_ms_ = get_parameter("tcp_ack_timeout_ms").as_int();
    max_retries_ = get_parameter("tcp_max_retries").as_int();
    std::string ros_topic = get_parameter("ros_topic").as_string();
    
    RCLCPP_INFO(get_logger(), "Initializing TCP connection to %s:%d", 
                server_host_.c_str(), server_port_);
    
    // Setup TCP connection
    setup_tcp_connection();
    
    // Create ROS subscription
    subscription_ = create_subscription<std_msgs::msg::String>(
      ros_topic, 10,
      [this](std_msgs::msg::String::UniquePtr msg) {
        RCLCPP_INFO(this->get_logger(), "Received from ROS: '%s'", msg->data.c_str());
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
        RCLCPP_ERROR(get_logger(), "Failed to create socket: %s", strerror(errno));
        return false;
      }
      
      // Set up server address
      struct sockaddr_in server_addr;
      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(server_port_);
      
      if (inet_pton(AF_INET, server_host_.c_str(), &server_addr.sin_addr) <= 0) {
        RCLCPP_ERROR(get_logger(), "Invalid address: %s", strerror(errno));
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
      }
      
      RCLCPP_INFO(get_logger(), "Connecting to TCP server...");
      
      // Connect to server
      if (connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        RCLCPP_ERROR(get_logger(), "Connection failed: %s", strerror(errno));
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
      }
      
      // Set socket to non-blocking mode for timeout handling
      int flags = fcntl(socket_fd_, F_GETFL, 0);
      fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK);
      
      RCLCPP_INFO(get_logger(), "Connected to TCP server successfully");
      connection_active_ = true;
      return true;
    }
    catch (const std::exception& ex) {
      RCLCPP_ERROR(get_logger(), "TCP connection setup failed: %s", ex.what());
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
      RCLCPP_WARN(get_logger(), "TCP connection not active, trying to reconnect...");
      if (!setup_tcp_connection()) {
        RCLCPP_ERROR(get_logger(), "Failed to reconnect to TCP server");
        return false;
      }
    }
    
    // Add a simple protocol: message length + message
    std::string framed_message = std::to_string(message.length()) + ":" + message + "\n";
    
    for (int retry = 0; retry <= max_retries_; ++retry) {
      try {
        if (retry > 0) {
          RCLCPP_WARN(get_logger(), "Retrying send (attempt %d of %d)...", 
                      retry, max_retries_);
        }
        
        // Send the message
        ssize_t bytes_sent = send(socket_fd_, framed_message.c_str(), framed_message.length(), 0);
        if (bytes_sent < 0) {
          if (errno == EWOULDBLOCK || errno == EAGAIN) {
            RCLCPP_WARN(get_logger(), "Send would block, retrying...");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
          } else {
            RCLCPP_ERROR(get_logger(), "Send failed: %s", strerror(errno));
            connection_active_ = false;
            return false;
          }
        } else if (bytes_sent == 0) {
          RCLCPP_ERROR(get_logger(), "Connection closed by peer");
          connection_active_ = false;
          return false;
        } else if (static_cast<size_t>(bytes_sent) < framed_message.length()) {
          RCLCPP_WARN(get_logger(), "Partial send: %zd of %zu bytes", 
                      bytes_sent, framed_message.length());
          // For simplicity, we'll just consider this a failure and retry
          continue;
        }
        
        // Message sent successfully, wait for ACK
        RCLCPP_INFO(get_logger(), "Sent to TCP server: %s", message.c_str());
        
        if (receive_ack()) {
          RCLCPP_INFO(get_logger(), "Received ACK from server");
          return true;
        } else {
          RCLCPP_WARN(get_logger(), "No ACK received, will retry");
        }
      }
      catch (const std::exception& ex) {
        RCLCPP_ERROR(get_logger(), "Send failed with exception: %s", ex.what());
        connection_active_ = false;
      }
    }
    
    RCLCPP_ERROR(get_logger(), "Failed to send message after %d retries", max_retries_);
    return false;
  }
  
  bool receive_ack() {
    struct pollfd pfd;
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;
    
    // Wait for data with timeout
    int poll_result = poll(&pfd, 1, ack_timeout_ms_);
    
    if (poll_result < 0) {
      RCLCPP_ERROR(get_logger(), "Poll failed: %s", strerror(errno));
      return false;
    } else if (poll_result == 0) {
      // Timeout
      RCLCPP_WARN(get_logger(), "ACK timeout");
      return false;
    }
    
    // Data is available, read the ACK
    char ack_buffer[32];
    ssize_t bytes_read = recv(socket_fd_, ack_buffer, sizeof(ack_buffer) - 1, 0);
    
    if (bytes_read < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        RCLCPP_WARN(get_logger(), "No data available");
        return false;
      } else {
        RCLCPP_ERROR(get_logger(), "Receive failed: %s", strerror(errno));
        connection_active_ = false;
        return false;
      }
    } else if (bytes_read == 0) {
      RCLCPP_ERROR(get_logger(), "Connection closed by peer");
      connection_active_ = false;
      return false;
    }
    
    // Null-terminate the received data
    ack_buffer[bytes_read] = '\0';
    
    // Simple ACK check - you can implement a more sophisticated protocol
    std::string ack_str(ack_buffer);
    return ack_str.find("ACK") != std::string::npos;
  }
  
  void close_connection() {
    if (socket_fd_ >= 0) {
      RCLCPP_INFO(get_logger(), "Closing TCP connection...");
      
      // Send a graceful disconnect message if connection is still active
      if (connection_active_) {
        const char* bye_msg = "BYE\n";
        send(socket_fd_, bye_msg, strlen(bye_msg), 0);
      }
      
      close(socket_fd_);
      socket_fd_ = -1;
      connection_active_ = false;
      RCLCPP_INFO(get_logger(), "TCP connection closed");
    }
  }

  // ROS subscription
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
  
  // TCP variables
  int socket_fd_;
  std::string server_host_;
  int server_port_;
  int ack_timeout_ms_;
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