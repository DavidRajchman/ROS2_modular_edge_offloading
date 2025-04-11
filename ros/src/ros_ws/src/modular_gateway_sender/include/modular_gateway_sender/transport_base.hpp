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
};

// TCP transport implementation
class TcpTransport : public TransportBase {
public:
  TcpTransport(const std::string& host, int port, int max_retries = 3);
  virtual ~TcpTransport();
  
  bool connect() override;
  void disconnect() override;
  bool send_data(const void* data, size_t size) override;
  bool is_connected() const override;
  
private:
  std::string server_host_;
  int server_port_;
  int max_retries_;
  int socket_fd_;
  bool connected_;
};

} // namespace gateway

#endif // TRANSPORT_BASE_HPP