#ifndef TRANSPORT_BASE_HPP
#define TRANSPORT_BASE_HPP

#include <memory>
#include <string>
#include <vector>
#include <cstdint>

namespace gateway {

// Base class for all transport methods (TCP, UDP)
class TransportBase {
public:
  virtual ~TransportBase() = default;
  
  // Connect to remote endpoint
  virtual bool connect() = 0;
  
  // Disconnect from remote endpoint
  virtual void disconnect() = 0;
  
  // Send raw data
  virtual bool send_data(const void* data, size_t size) = 0;
  
  // Check if connected
  virtual bool is_connected() const = 0;

  //----receive methods---- 

  // Check if data is available to read with optional timeout (in milliseconds)
  // Returns true if data is available, false otherwise
  virtual bool data_available(int timeout_ms = 0) = 0;
  
  // Receive data into provided buffer
  // Returns number of bytes received, 0 on connection closed, or -1 on error
  virtual int receive_data(void* buffer, size_t max_size) = 0;
  
  // Receive exactly the specified number of bytes (blocks until complete)
  // Returns true if successful, false on error or connection closed
  virtual bool receive_exact(void* buffer, size_t size) = 0;

};

// TCP transport implementation
class TcpClientTransport : public TransportBase {
public:
  TcpClientTransport(const std::string& host, int port, int max_retries = 3);
  virtual ~TcpClientTransport();
  
  // Connection management
  bool connect() override;
  void disconnect() override;
  bool is_connected() const override;
  
  // Data transmission
  bool send_data(const void* data, size_t size) override;
  
  // Data reception
  bool data_available(int timeout_ms = 0) override;
  int receive_data(void* buffer, size_t max_size) override;
  bool receive_exact(void* buffer, size_t size) override;
  
private:
  std::string server_host_;
  int server_port_;
  int max_retries_;
  int socket_fd_;
  bool connected_;
  rclcpp::Logger logger_{rclcpp::get_logger("tcp_transport")};
};

// TCP Server transport implementation
class TcpServerTransport : public TransportBase {
public:
  TcpServerTransport(int port, int max_connections = 1);
  virtual ~TcpServerTransport();
  
  // Connection management (for server this means start/stop listening)
  bool connect() override;  // For server: start listening
  void disconnect() override;  // For server: stop listening and close connections
  bool is_connected() const override;  // True if client is connected
  
  // Data transmission to connected client
  bool send_data(const void* data, size_t size) override;
  
  // Data reception from connected client
  bool data_available(int timeout_ms = 0) override;
  int receive_data(void* buffer, size_t max_size) override;
  bool receive_exact(void* buffer, size_t size) override;
  
private:
  int port_;               // Server listening port
  int max_connections_;    // Maximum allowed connections (typically 1)
  int server_socket_fd_;   // Socket for listening
  int client_socket_fd_;   // Socket for accepted client
  bool listening_;         // Server is currently listening
  bool client_connected_;  // Client is currently connected
  rclcpp::Logger logger_{rclcpp::get_logger("tcp_server_transport")};
  
  // Accept a new client connection (called internally)
  bool accept_connection();
};

} // namespace gateway
#endif // TRANSPORT_BASE_HPP