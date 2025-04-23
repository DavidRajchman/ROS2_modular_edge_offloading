#include "modular_gateway_sender/logging_utils.hpp"

#include "modular_gateway_sender/ros_gateway.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <cerrno>
#include <stdexcept>

namespace gateway {

TcpClientTransport::TcpClientTransport(const std::string& host, int port, int max_retries)
  : server_host_(host), 
    server_port_(port), 
    max_retries_(max_retries), 
    socket_fd_(-1), 
    connected_(false)
{
}

TcpClientTransport::~TcpClientTransport()
{
  disconnect();
}

bool TcpClientTransport::connect()
{
  // Close existing connection if any
  if (socket_fd_ >= 0) {
    close(socket_fd_);
    socket_fd_ = -1;
    connected_ = false;
  }
  
  try {
    // Create socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
      throw std::runtime_error(std::string("Failed to create socket: ") + strerror(errno));
    }
    
    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port_);
    
    if (inet_pton(AF_INET, server_host_.c_str(), &server_addr.sin_addr) <= 0) {
      throw std::runtime_error(std::string("Invalid address: ") + strerror(errno));
    }
    
    // Connect to server
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      throw std::runtime_error(std::string("Connection failed: ") + strerror(errno));
    }
    
    connected_ = true;
    return true;
  }
  catch (const std::exception& ex) {
    if (socket_fd_ >= 0) {
      close(socket_fd_);
      socket_fd_ = -1;
    }
    connected_ = false;
    return false;
  }
}

void TcpClientTransport::disconnect()
{
  if (socket_fd_ >= 0) {
    close(socket_fd_);
    socket_fd_ = -1;
    connected_ = false;
  }
}

bool TcpClientTransport::send_data(const void* data, size_t size)
{
  if (!connected_ || socket_fd_ < 0) {
    return false;
  }
  
  const uint8_t* buffer = static_cast<const uint8_t*>(data);
  size_t remaining = size;
  size_t offset = 0;
  
  // Retry logic
  for (int retry = 0; retry <= max_retries_; ++retry) {
    try {
      if (retry > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
      
      while (remaining > 0) {
        ssize_t bytes_sent = ::send(socket_fd_, buffer + offset, remaining, 0);
        if (bytes_sent < 0) {
          if (errno == EWOULDBLOCK || errno == EAGAIN) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
          } else {
            throw std::runtime_error(std::string("Send failed: ") + strerror(errno));
          }
        } else if (bytes_sent == 0) {
          throw std::runtime_error("Connection closed by peer");
        }
        
        offset += bytes_sent;
        remaining -= bytes_sent;
      }
      
      return true;
    }
    catch (const std::exception&) {
      if (retry == max_retries_) {
        connected_ = false;
        return false;
      }
    }
  }
  
  return false;
}

bool TcpClientTransport::is_connected() const
{
  return connected_ && socket_fd_ >= 0;
}

bool TcpClientTransport::data_available(int timeout_ms) {
    if (!is_connected()) return false;
    
    fd_set readfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    FD_SET(socket_fd_, &readfds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(socket_fd_ + 1, &readfds, NULL, NULL, 
                       timeout_ms > 0 ? &tv : NULL);
    
    if (result < 0) {
        LOG_ERROR(logger_, "Select error: %s", strerror(errno));
        return false;
    }
    
    return result > 0;
}

int TcpClientTransport::receive_data(void* buffer, size_t size) {
    if (!is_connected()) return -1;
    
    ssize_t bytes_received = recv(socket_fd_, buffer, size, 0);
    
    if (bytes_received < 0) {
        LOG_ERROR(logger_, "Receive error: %s", strerror(errno));
        return -1;
    }
    
    // Connection closed by peer
    if (bytes_received == 0) {
        LOG_WARN(logger_, "Connection closed by peer");
        disconnect();
        return -1;
    }
    
    return bytes_received;
}

bool TcpClientTransport::receive_exact(void* buffer, size_t size) {
    if (!is_connected()) return false;
    
    size_t total_received = 0;
    char* buf_ptr = static_cast<char*>(buffer);
    
    while (total_received < size) {
        ssize_t bytes_received = recv(socket_fd_, 
                                     buf_ptr + total_received, 
                                     size - total_received, 0);
        
        if (bytes_received < 0) {
            LOG_ERROR(logger_, "Receive error: %s", strerror(errno));
            return false;
        }
        
        // Connection closed by peer
        if (bytes_received == 0) {
            LOG_WARN(logger_, "Connection closed by peer during receive");
            disconnect();
            return false;
        }
        
        total_received += bytes_received;
    }
    
    return true;
}

} // namespace gateway