#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h> // Added for TCP_NODELAY
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <chrono>
#include <thread>
#include <cerrno>
#include <stdexcept>

namespace gateway {

TcpServerTransport::TcpServerTransport(int port, int max_connections)
  : port_(port), 
    max_connections_(max_connections), 
    server_socket_fd_(-1), 
    client_socket_fd_(-1),
    listening_(false),
    client_connected_(false)
{
}

TcpServerTransport::~TcpServerTransport()
{
  disconnect();
}

bool TcpServerTransport::connect()
{
  // This method starts the server listening for connections
  
  // Close existing sockets if any
  disconnect();
  
  try {
    // Create the server socket
    server_socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd_ < 0) {
      throw std::runtime_error(std::string("Failed to create socket: ") + strerror(errno));
    }
    
    // Set socket options to allow address reuse
    int opt = 1;
    if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      throw std::runtime_error(std::string("Failed to set socket options: ") + strerror(errno));
    }
    
    // Enable TCP keepalive to detect disconnected clients
    if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
      LOG_WARN(logger_, "Failed to set keepalive: %s", strerror(errno));
      // Not critical, can continue
    }
    
    // Bind to port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Listen on all available interfaces
    server_addr.sin_port = htons(port_);
    
    if (bind(server_socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      throw std::runtime_error(std::string("Bind failed: ") + strerror(errno));
    }
    
    // Listen for connections
    if (listen(server_socket_fd_, max_connections_) < 0) {
      throw std::runtime_error(std::string("Listen failed: ") + strerror(errno));
    }
    
    // Make the server socket non-blocking for accept()
    int flags = fcntl(server_socket_fd_, F_GETFL, 0);
    if (flags == -1) {
      throw std::runtime_error(std::string("Failed to get socket flags: ") + strerror(errno));
    }
    if (fcntl(server_socket_fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
      throw std::runtime_error(std::string("Failed to set socket non-blocking: ") + strerror(errno));
    }
    
    listening_ = true;
    
    // Print the actual socket we're bound to
    struct sockaddr_in actual_addr;
    socklen_t addr_len = sizeof(actual_addr);
    if (getsockname(server_socket_fd_, (struct sockaddr*)&actual_addr, &addr_len) == 0) {
      char host[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &actual_addr.sin_addr, host, INET_ADDRSTRLEN);
      LOG_INFO(logger_, "TCP server listening on %s:%d", 
               host, ntohs(actual_addr.sin_port));
    } else {
      LOG_INFO(logger_, "TCP server listening on port %d", port_);
    }
    
    return true;
  }
  catch (const std::exception& ex) {
    if (server_socket_fd_ >= 0) {
      close(server_socket_fd_);
      server_socket_fd_ = -1;
    }
    listening_ = false;
    client_connected_ = false;
    LOG_ERROR(logger_, "Failed to start server: %s", ex.what());
    return false;
  }
}

void TcpServerTransport::disconnect()
{
  // Close client socket if connected
  if (client_socket_fd_ >= 0) {
    close(client_socket_fd_);
    client_socket_fd_ = -1;
    client_connected_ = false;
    LOG_INFO(logger_, "Client connection closed");
  }
  
  // Close server socket if listening
  if (server_socket_fd_ >= 0) {
    close(server_socket_fd_);
    server_socket_fd_ = -1;
    listening_ = false;
    LOG_INFO(logger_, "Server stopped listening");
  }
}

bool TcpServerTransport::is_connected() const
{
  // For server transport, consider connected if either:
  // 1. We have an active client connection
  // 2. We're listening for connections (but no client yet)
  return client_connected_ || listening_;
}

bool TcpServerTransport::accept_connection()
{
  if (!listening_ || server_socket_fd_ < 0) {
    return false;
  }
  
  // If already connected to a client, check if the connection is still alive
  if (client_connected_ && client_socket_fd_ >= 0) {
    // Simple connection check - send 0 bytes
    // MSG_NOSIGNAL prevents SIGPIPE if client disconnected abruptly
    if (::send(client_socket_fd_, nullptr, 0, MSG_NOSIGNAL) < 0) {
      if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN || errno == EBADF) {
        LOG_WARN(logger_, "Client disconnected (checked via send): %s", strerror(errno));
        close(client_socket_fd_);
        client_socket_fd_ = -1;
        client_connected_ = false;
      }
    } else {
      // Connection still good
      return true;
    }
  }
  
  // Try to accept a connection
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  
  int new_socket = accept(server_socket_fd_, (struct sockaddr*)&client_addr, &client_len);
  if (new_socket < 0) {
    // Non-blocking accept, so EAGAIN/EWOULDBLOCK means no connection pending
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false; // No client waiting, not an error
    }
    LOG_ERROR(logger_, "Accept failed: %s", strerror(errno));
    return false;
  }
  
  // We have a new client
  client_socket_fd_ = new_socket;
  client_connected_ = true;
  
  // Enable TCP keepalive for the client socket too
  int opt = 1;
  if (setsockopt(client_socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
    LOG_WARN(logger_, "Failed to set client keepalive: %s", strerror(errno));
    // Not critical, can continue
  }

  // Set TCP_NODELAY for the client socket
  if (setsockopt(client_socket_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt)) < 0) {
    LOG_WARN(logger_, "Failed to set TCP_NODELAY on client socket: %s", strerror(errno));
    // Not critical, can continue
  }
  
  // Make the client socket non-blocking for better performance
  int flags = fcntl(client_socket_fd_, F_GETFL, 0);
  if (flags != -1) {
    fcntl(client_socket_fd_, F_SETFL, flags | O_NONBLOCK);
  } else {
    LOG_WARN(logger_, "Failed to get client socket flags for O_NONBLOCK: %s", strerror(errno));
  }
  
  // Get the client's IP address as string
  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  LOG_INFO(logger_, "Client connected from %s:%d", client_ip, ntohs(client_addr.sin_port));
  
  return true;
}

// Helper function to wait for socket readiness
inline int wait_for_fd(int fd, bool check_read, bool check_write, long timeout_us) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv;
    tv.tv_sec = timeout_us / 1000000;
    tv.tv_usec = timeout_us % 1000000;

    fd_set* read_fds = check_read ? &fds : nullptr;
    fd_set* write_fds = check_write ? &fds : nullptr;
    
    return select(fd + 1, read_fds, write_fds, nullptr, &tv);
}



bool TcpServerTransport::send_data(const void* data, size_t size)
{
  // Try to accept connection a few times with yields if no client
  const int max_initial_accept_attempts = 5; // Reduced attempts
  for (int attempt = 0; attempt < max_initial_accept_attempts; attempt++) {
    accept_connection();
    
    if (client_connected_ && client_socket_fd_ >= 0) {
      break;
    }
    
    if (attempt < max_initial_accept_attempts - 1) {
      // Yield to allow other threads (like receiver trying to accept) to run
      std::this_thread::yield(); 
      LOG_DEBUG(logger_, "No client connected for send, yielding before retry %d/%d", 
               attempt + 1, max_initial_accept_attempts);
    }
  }
  
  if (!client_connected_ || client_socket_fd_ < 0) {
    LOG_WARN(logger_, "No client connected after initial attempts, cannot send data");
    return false;
  }
  
  const uint8_t* buffer = static_cast<const uint8_t*>(data);
  size_t remaining = size;
  size_t offset = 0;
  
  const int max_send_attempts = 100; // Increased attempts due to shorter waits
  int send_attempts = 0;
  const long select_timeout_us = 50; // 50 microseconds for select
  
  while (remaining > 0 && send_attempts < max_send_attempts) {
    ssize_t bytes_sent = ::send(client_socket_fd_, buffer + offset, remaining, MSG_NOSIGNAL);
    
    if (bytes_sent < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        int select_res = wait_for_fd(client_socket_fd_, false, true, select_timeout_us);
        if (select_res < 0) { // select error
            LOG_ERROR(logger_, "select() error during send: %s", strerror(errno));
            // Consider it a fatal error for this send operation
            close(client_socket_fd_);
            client_socket_fd_ = -1;
            client_connected_ = false;
            return false;
        }
        if (select_res == 0) { // timeout
            std::this_thread::yield();
        }
        // if select_res > 0, socket is writable, loop will retry send
        send_attempts++;
        continue;
      } else if (errno == EPIPE || errno == ECONNRESET) {
        LOG_WARN(logger_, "Client disconnected during send: %s", strerror(errno));
        close(client_socket_fd_);
        client_socket_fd_ = -1;
        client_connected_ = false;
        return false;
      } else {
        LOG_ERROR(logger_, "Send failed: %s", strerror(errno));
        // For other errors, we might also consider the connection lost
        close(client_socket_fd_);
        client_socket_fd_ = -1;
        client_connected_ = false;
        return false;
      }
    } else if (bytes_sent == 0) {
      LOG_WARN(logger_, "Send returned 0 bytes, possible client disconnect or invalid state");
      // Treat as a condition to retry with select/yield
      std::this_thread::yield();
      send_attempts++;
      continue;
    }
    
    offset += bytes_sent;
    remaining -= bytes_sent;
    send_attempts = 0; 
  }
  
  if (remaining > 0) {
    LOG_ERROR(logger_, "Send timed out after %d attempts, %zu bytes remaining", max_send_attempts, remaining);
    return false;
  }
  
  return true;
}

bool TcpServerTransport::data_available(int timeout_ms)
{
  accept_connection(); // Check for new/lost connections
  
  if (!client_connected_ || client_socket_fd_ < 0) {
    // No client connected, so no data available from a client.
    // Removed sleep here; RosGateway's receiver_thread_func will handle polling sleep.
    return false;
  }
  
  // Use the helper, ensuring timeout_ms is converted to microseconds
  long timeout_us = static_cast<long>(timeout_ms) * 1000;
  if (timeout_us < 0) timeout_us = 0; // Ensure non-negative timeout for select

  int result = wait_for_fd(client_socket_fd_, true, false, timeout_us);
  
  if (result < 0) {
    if (errno == EINTR) {
      return false;
    }
    LOG_ERROR(logger_, "Select error in data_available: %s", strerror(errno));
    if (errno == EBADF) {
      LOG_WARN(logger_, "Client socket is no longer valid in data_available");
      close(client_socket_fd_);
      client_socket_fd_ = -1;
      client_connected_ = false;
    }
    return false;
  }
  
  return result > 0;
}

int TcpServerTransport::receive_data(void* buffer, size_t max_size)
{
  // Try to accept a new connection first
  accept_connection();
  
  if (!client_connected_ || client_socket_fd_ < 0) {
    return -1;
  }
  
  ssize_t bytes_received = recv(client_socket_fd_, buffer, max_size, 0);
  
  if (bytes_received < 0) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      // No data available right now, not an error
      return 0;
    }
    LOG_ERROR(logger_, "Receive error: %s", strerror(errno));
    return -1;
  }
  
  // Connection closed by client
  if (bytes_received == 0) {
    LOG_WARN(logger_, "Connection closed by client");
    close(client_socket_fd_);
    client_socket_fd_ = -1;
    client_connected_ = false;
    return -1;
  }
  
  return bytes_received;
}

bool TcpServerTransport::receive_exact(void* buffer, size_t size)
{
  // It's important that accept_connection() is efficient and doesn't block for long.
  // The current implementation of accept_connection checks existing connection first.
  accept_connection(); 
  
  if (!client_connected_ || client_socket_fd_ < 0) {
    LOG_DEBUG(logger_, "receive_exact: No client connected.");
    return false;
  }
  
  size_t total_received = 0;
  char* buf_ptr = static_cast<char*>(buffer);
  
  const int max_recv_attempts = 100; // Increased attempts due to shorter waits
  int recv_attempts = 0;
  const long select_timeout_us = 50; // 50 microseconds for select

  while (total_received < size && recv_attempts < max_recv_attempts) {
    ssize_t bytes_received = recv(client_socket_fd_, 
                                 buf_ptr + total_received, 
                                 size - total_received, 0); // MSG_DONTWAIT could also be used here
                                                             // as socket is non-blocking, but 0 is fine.
    
    if (bytes_received < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        int select_res = wait_for_fd(client_socket_fd_, true, false, select_timeout_us);
        if (select_res < 0) { // select error
            LOG_ERROR(logger_, "select() error during receive_exact: %s", strerror(errno));
            // Consider it a fatal error for this receive operation
            // No need to close socket here, error will propagate up
            return false; 
        }
        if (select_res == 0) { // timeout
            std::this_thread::yield();
        }
        // if select_res > 0, socket is readable, loop will retry recv
        recv_attempts++;
        continue;
      }
      
      LOG_ERROR(logger_, "Receive error in receive_exact: %s", strerror(errno));
      // For other errors, the connection might be compromised.
      // Let higher level decide if disconnect is needed based on return false.
      return false;
    }
    
    if (bytes_received == 0) {
      LOG_WARN(logger_, "Connection closed by client during receive_exact (received 0 bytes)");
      close(client_socket_fd_);
      client_socket_fd_ = -1;
      client_connected_ = false;
      return false;
    }
    
    total_received += bytes_received;
    recv_attempts = 0; // Reset on progress
  }
  
  if (total_received < size) {
    LOG_ERROR(logger_, "Receive_exact timed out after %d attempts, got %zu of %zu bytes", 
              max_recv_attempts, total_received, size);
    return false;
  }
  
  return true;
}

} // namespace gateway