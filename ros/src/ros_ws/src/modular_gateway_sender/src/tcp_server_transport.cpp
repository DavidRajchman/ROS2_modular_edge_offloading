#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/transport_base.hpp"
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
    fcntl(server_socket_fd_, F_SETFL, flags | O_NONBLOCK);
    
    listening_ = true;
    LOG_INFO(logger_, "TCP server listening on port %d", port_);
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
  }
  
  // Close server socket if listening
  if (server_socket_fd_ >= 0) {
    close(server_socket_fd_);
    server_socket_fd_ = -1;
    listening_ = false;
  }
}

bool TcpServerTransport::is_connected() const
{
  return client_connected_ && client_socket_fd_ >= 0;
}

bool TcpServerTransport::accept_connection()
{
  if (!listening_ || server_socket_fd_ < 0) {
    return false;
  }
  
  // If already connected to a client, don't accept a new one
  if (client_connected_ && client_socket_fd_ >= 0) {
    return true;
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
  
  // Get the client's IP address as string
  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  LOG_INFO(logger_, "Client connected from %s:%d", client_ip, ntohs(client_addr.sin_port));
  
  return true;
}

bool TcpServerTransport::send_data(const void* data, size_t size)
{
  // Accept any pending connections before sending
  accept_connection();
  
  if (!client_connected_ || client_socket_fd_ < 0) {
    return false;
  }
  
  const uint8_t* buffer = static_cast<const uint8_t*>(data);
  size_t remaining = size;
  size_t offset = 0;
  
  while (remaining > 0) {
    ssize_t bytes_sent = ::send(client_socket_fd_, buffer + offset, remaining, 0);
    if (bytes_sent < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      } else {
        LOG_ERROR(logger_, "Send failed: %s", strerror(errno));
        // Assume client disconnected
        close(client_socket_fd_);
        client_socket_fd_ = -1;
        client_connected_ = false;
        return false;
      }
    } else if (bytes_sent == 0) {
      LOG_WARN(logger_, "Connection closed by client during send");
      close(client_socket_fd_);
      client_socket_fd_ = -1;
      client_connected_ = false;
      return false;
    }
    
    offset += bytes_sent;
    remaining -= bytes_sent;
  }
  
  return true;
}

bool TcpServerTransport::data_available(int timeout_ms)
{
  // Try to accept a new connection first
  accept_connection();
  
  if (!client_connected_ || client_socket_fd_ < 0) {
    return false;
  }
  
  fd_set readfds;
  struct timeval tv;
  
  FD_ZERO(&readfds);
  FD_SET(client_socket_fd_, &readfds);
  
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  
  int result = select(client_socket_fd_ + 1, &readfds, NULL, NULL, 
                     timeout_ms > 0 ? &tv : NULL);
  
  if (result < 0) {
    LOG_ERROR(logger_, "Select error: %s", strerror(errno));
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
  // Try to accept a new connection first
  accept_connection();
  
  if (!client_connected_ || client_socket_fd_ < 0) {
    return false;
  }
  
  size_t total_received = 0;
  char* buf_ptr = static_cast<char*>(buffer);
  
  while (total_received < size) {
    ssize_t bytes_received = recv(client_socket_fd_, 
                                 buf_ptr + total_received, 
                                 size - total_received, 0);
    
    if (bytes_received < 0) {
      LOG_ERROR(logger_, "Receive error: %s", strerror(errno));
      return false;
    }
    
    // Connection closed by client
    if (bytes_received == 0) {
      LOG_WARN(logger_, "Connection closed by client during receive");
      close(client_socket_fd_);
      client_socket_fd_ = -1;
      client_connected_ = false;
      return false;
    }
    
    total_received += bytes_received;
  }
  
  return true;
}

} // namespace gateway