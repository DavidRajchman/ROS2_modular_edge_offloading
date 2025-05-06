#include "transport/logging_utils.hpp" // Changed from modular_gateway_sender/
#include "transport/transport_base.hpp" // Changed from modular_gateway_sender/

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
      // Log connection failure before throwing
      LOG_ERROR("Connection failed to %s:%d - %s", server_host_.c_str(), server_port_, strerror(errno));
      throw std::runtime_error(std::string("Connection failed: ") + strerror(errno));
    }
    
    connected_ = true;
    LOG_INFO("Successfully connected to %s:%d", server_host_.c_str(), server_port_);
    return true;
  }
  catch (const std::exception& ex) {
    if (socket_fd_ >= 0) {
      close(socket_fd_);
      socket_fd_ = -1;
    }
    connected_ = false;
    // Log the exception message if not already logged by a specific LOG_ERROR above
    // For example, socket creation or inet_pton errors
    if (strstr(ex.what(), "Connection failed") == nullptr) { // Avoid double logging connection failure
        LOG_ERROR("Failed to connect: %s", ex.what());
    }
    return false;
  }
}

void TcpClientTransport::disconnect()
{
  if (socket_fd_ >= 0) {
    LOG_INFO("Disconnecting from %s:%d", server_host_.c_str(), server_port_);
    close(socket_fd_);
    socket_fd_ = -1;
    connected_ = false;
  }
}

bool TcpClientTransport::send_data(const void* data, size_t size)
{
  if (!connected_ || socket_fd_ < 0) {
    LOG_WARN("Cannot send data: not connected.");
    return false;
  }
  
  const uint8_t* buffer = static_cast<const uint8_t*>(data);
  size_t remaining = size;
  size_t offset = 0;
  
  // Retry logic
  for (int retry = 0; retry <= max_retries_; ++retry) {
    try {
      if (retry > 0) {
        LOG_DEBUG("Retrying send operation (attempt %d/%d) to %s:%d", retry, max_retries_, server_host_.c_str(), server_port_);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Consider making delay configurable
      }
      
      while (remaining > 0) {
        ssize_t bytes_sent = ::send(socket_fd_, buffer + offset, remaining, 0); // MSG_NOSIGNAL could be useful here too
        if (bytes_sent < 0) {
          if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // This case implies non-blocking socket, which is not set up in connect() for client
            // If socket is blocking, EWOULDBLOCK/EAGAIN shouldn't happen unless a timeout is set via setsockopt
            LOG_DEBUG("Send would block, retrying shortly.");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
          } else {
            LOG_ERROR("Send failed to %s:%d - %s", server_host_.c_str(), server_port_, strerror(errno));
            throw std::runtime_error(std::string("Send failed: ") + strerror(errno));
          }
        } else if (bytes_sent == 0) {
          // According to send(2), this means the peer has performed an orderly shutdown if the socket is stream-oriented.
          LOG_WARN("Connection closed by peer %s:%d during send.", server_host_.c_str(), server_port_);
          throw std::runtime_error("Connection closed by peer");
        }
        
        offset += bytes_sent;
        remaining -= bytes_sent;
      }
      
      LOG_DEBUG("Successfully sent %zu bytes to %s:%d", size, server_host_.c_str(), server_port_);
      return true;
    }
    catch (const std::exception& ex) {
      LOG_WARN("Exception during send (attempt %d/%d): %s", retry, max_retries_, ex.what());
      disconnect(); // Disconnect on send failure
      if (retry < max_retries_) {
        LOG_INFO("Attempting to reconnect to %s:%d", server_host_.c_str(), server_port_);
        if (!connect()) { // Try to reconnect
          LOG_ERROR("Reconnect attempt %d failed.", retry);
          if (retry == max_retries_ -1) { // if this was the second to last attempt overall
             LOG_ERROR("Final reconnect attempt failed. Giving up.");
             return false;
          }
          // continue to next retry iteration which will sleep
        } else {
           LOG_INFO("Reconnected successfully to %s:%d. Retrying send.", server_host_.c_str(), server_port_);
           // Reset remaining and offset for the new attempt with the new connection
           remaining = size;
           offset = 0;
        }
      } else { // This was the last retry
        LOG_ERROR("All send retries failed for %s:%d.", server_host_.c_str(), server_port_);
        return false;
      }
    }
  }
  
  return false; // Should be unreachable if logic is correct, but as a fallback
}

bool TcpClientTransport::is_connected() const
{
  return connected_ && socket_fd_ >= 0;
}

bool TcpClientTransport::data_available(int timeout_ms) {
    if (!is_connected()) {
        LOG_DEBUG("Cannot check for data: not connected.");
        return false;
    }
    
    fd_set readfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    FD_SET(socket_fd_, &readfds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(socket_fd_ + 1, &readfds, NULL, NULL, 
                       timeout_ms >= 0 ? &tv : NULL); // Allow negative timeout for indefinite block
    
    if (result < 0) {
        if (errno == EINTR) { // Interrupted by signal
            LOG_DEBUG("Select call interrupted by signal.");
            return false; 
        }
        LOG_ERROR("Select error on socket for %s:%d - %s", server_host_.c_str(), server_port_, strerror(errno));
        // Consider disconnecting if EBADF, etc.
        if (errno == EBADF) {
            LOG_WARN("Socket descriptor %d is bad, disconnecting.", socket_fd_);
            disconnect();
        }
        return false;
    }
    
    return result > 0; // True if one or more descriptors are ready
}

int TcpClientTransport::receive_data(void* buffer, size_t max_size) {
    if (!is_connected()) {
        LOG_WARN("Cannot receive data: not connected.");
        return -1;
    }
    
    ssize_t bytes_received = recv(socket_fd_, buffer, max_size, 0);
    
    if (bytes_received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            // This implies non-blocking socket or timeout set on socket.
            // For a simple blocking receive, this shouldn't be hit unless socket options were changed.
            LOG_DEBUG("Receive would block or timed out (EWOULDBLOCK/EAGAIN). No data received.");
            return 0; // Indicate no data, not an error
        }
        LOG_ERROR("Receive error on socket for %s:%d - %s", server_host_.c_str(), server_port_, strerror(errno));
        disconnect(); // Disconnect on critical receive error
        return -1;
    }
    
    // Connection closed by peer
    if (bytes_received == 0) {
        LOG_WARN("Connection closed by peer %s:%d during receive.", server_host_.c_str(), server_port_);
        disconnect();
        return -1; // Indicate connection closed
    }
    
    LOG_DEBUG("Received %zd bytes from %s:%d", bytes_received, server_host_.c_str(), server_port_);
    return static_cast<int>(bytes_received);
}

bool TcpClientTransport::receive_exact(void* buffer, size_t size) {
    if (!is_connected()) {
        LOG_WARN("Cannot receive exact data: not connected.");
        return false;
    }
    if (size == 0) {
        return true; // Nothing to receive
    }
    
    size_t total_received = 0;
    char* buf_ptr = static_cast<char*>(buffer);
    
    // Add a timeout mechanism for receive_exact to prevent indefinite blocking
    // This could be a simple attempt counter with sleeps, or use select/poll before each recv
    const int max_recv_attempts = 50; // Example: 50 * 10ms = 500ms timeout
    int recv_attempts = 0;

    while (total_received < size) {
        ssize_t bytes_received = recv(socket_fd_, 
                                     buf_ptr + total_received, 
                                     size - total_received, 0); // Potentially MSG_WAITALL if appropriate and understood
        
        if (bytes_received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // Socket is non-blocking and no data, or SO_RCVTIMEO expired
                if (recv_attempts < max_recv_attempts) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Wait a bit
                    recv_attempts++;
                    LOG_DEBUG("Receive would block, retrying exact receive (%d/%d)", recv_attempts, max_recv_attempts);
                    continue;
                } else {
                    LOG_ERROR("Receive timed out after %d attempts for exact data from %s:%d. Got %zu of %zu bytes.", max_recv_attempts, server_host_.c_str(), server_port_, total_received, size);
                    // Do not disconnect here, as partial data might be an application-level issue or timeout.
                    return false;
                }
            }
            LOG_ERROR("Receive error on socket for %s:%d - %s. Needed %zu, got %zu.", server_host_.c_str(), server_port_, strerror(errno), size, total_received);
            disconnect(); // Disconnect on critical receive error
            return false;
        }
        
        // Connection closed by peer
        if (bytes_received == 0) {
            LOG_WARN("Connection closed by peer %s:%d during receive_exact. Needed %zu, got %zu.", server_host_.c_str(), server_port_, size, total_received);
            disconnect();
            return false;
        }
        
        total_received += bytes_received;
        recv_attempts = 0; // Reset attempts on successful receive
        LOG_DEBUG("Received chunk: %zd bytes, total: %zu/%zu for exact receive from %s:%d", bytes_received, total_received, size, server_host_.c_str(), server_port_);
    }
    
    LOG_INFO("Successfully received exact %zu bytes from %s:%d", size, server_host_.c_str(), server_port_);
    return true;
}

} // namespace gateway