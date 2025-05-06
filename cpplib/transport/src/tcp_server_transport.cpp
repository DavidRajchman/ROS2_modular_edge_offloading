#include "transport/logging_utils.hpp" // Changed from modular_gateway_sender/
#include "transport/transport_base.hpp" // Changed from modular_gateway_sender/

#include <sys/socket.h>
#include <unistd.h> // For close
#include <fcntl.h>  // For fcntl
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdexcept>
#include <cstring>  // For strerror, memset
#include <cerrno>   // For errno
#include <thread>   // For std::this_thread::sleep_for
#include <chrono>   // For std::chrono::milliseconds

namespace gateway {

TcpServerTransport::TcpServerTransport(int port, int max_connections)
  : port_(port), 
    max_connections_(max_connections), 
    server_socket_fd_(-1), 
    client_socket_fd_(-1),
    listening_(false),
    client_connected_(false)
    // logger_ is implicitly used by old macros, not needed for new ones
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
      LOG_WARN("Failed to set keepalive: %s", strerror(errno));
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
      LOG_INFO("TCP server listening on %s:%d", 
               host, ntohs(actual_addr.sin_port));
    } else {
      LOG_INFO("TCP server listening on port %d", port_);
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
    LOG_ERROR("Failed to start server: %s", ex.what());
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
    LOG_INFO("Client connection closed");
  }
  
  // Close server socket if listening
  if (server_socket_fd_ >= 0) {
    close(server_socket_fd_);
    server_socket_fd_ = -1;
    listening_ = false;
    LOG_INFO("Server stopped listening");
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
    if (send(client_socket_fd_, nullptr, 0, MSG_NOSIGNAL) < 0) {
      if (errno == EPIPE || errno == ECONNRESET) {
        LOG_WARN("Client disconnected: %s", strerror(errno));
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
    LOG_ERROR("Accept failed: %s", strerror(errno));
    return false;
  }
  
  // We have a new client
  client_socket_fd_ = new_socket;
  client_connected_ = true;
  
  // Enable TCP keepalive for the client socket too
  int opt = 1;
  if (setsockopt(client_socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
    LOG_WARN("Failed to set client keepalive: %s", strerror(errno));
    // Not critical, can continue
  }
  
  // Make the client socket non-blocking for better performance
  int flags = fcntl(client_socket_fd_, F_GETFL, 0);
  if (flags != -1) {
    fcntl(client_socket_fd_, F_SETFL, flags | O_NONBLOCK);
  }
  
  // Get the client's IP address as string
  char client_ip[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  LOG_INFO("Client connected from %s:%d", client_ip, ntohs(client_addr.sin_port));
  
  return true;
}

bool TcpServerTransport::send_data(const void* data, size_t size)
{
  // Try to accept connection multiple times before giving up
  const int max_accept_attempts = 5;
  for (int attempt = 0; attempt < max_accept_attempts; attempt++) {
    // Try to accept any pending connections
    accept_connection();
    
    // If we have a client, proceed with sending
    if (client_connected_ && client_socket_fd_ >= 0) {
      break;
    }
    
    // No client yet, wait a bit before trying again
    if (attempt < max_accept_attempts - 1) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      LOG_DEBUG("No client connected, waiting before retry %d/%d", 
               attempt + 1, max_accept_attempts);
    }
  }
  
  // If still no client after attempts, fail
  if (!client_connected_ || client_socket_fd_ < 0) {
    LOG_WARN("No client connected, cannot send data");
    return false;
  }
  
  // Send logic with timeout
  const uint8_t* buffer = static_cast<const uint8_t*>(data);
  size_t remaining = size;
  size_t offset = 0;
  
  // Use a timeout to avoid hanging indefinitely
  const int max_send_attempts = 50; // ~500ms max wait
  int send_attempts = 0;
  
  while (remaining > 0 && send_attempts < max_send_attempts) {
    ssize_t bytes_sent = ::send(client_socket_fd_, buffer + offset, remaining, MSG_NOSIGNAL);
    
    if (bytes_sent < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        // Socket buffer is full, wait a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        send_attempts++;
        continue;
      } else if (errno == EPIPE || errno == ECONNRESET) {
        // Client disconnected
        LOG_WARN("Client disconnected during send: %s", strerror(errno));
        close(client_socket_fd_);
        client_socket_fd_ = -1;
        client_connected_ = false;
        return false;
      } else {
        // Other error
        LOG_ERROR("Send failed: %s", strerror(errno));
        close(client_socket_fd_);
        client_socket_fd_ = -1;
        client_connected_ = false;
        return false;
      }
    } else if (bytes_sent == 0) {
      // This shouldn't normally happen with non-blocking sockets
      LOG_WARN("Send returned 0 bytes, possible client disconnect");
      send_attempts++;
      continue;
    }
    
    // Successful send, update counters
    offset += bytes_sent;
    remaining -= bytes_sent;
    send_attempts = 0; // Reset attempts counter on progress
  }
  
  // Check if we timed out
  if (remaining > 0) {
    LOG_ERROR("Send timed out, %zu bytes remaining", remaining);
    return false;
  }
  
  return true;
}

bool TcpServerTransport::data_available(int timeout_ms)
{
  // Try to accept a new connection first
  accept_connection();
  
  if (!client_connected_ || client_socket_fd_ < 0) {
    // When no client is connected, don't block in select for too long
    // to give accept_connection() more frequent chances to run
    if (timeout_ms > 100) timeout_ms = 100;
    
    // No client, check for connections after a short delay
    std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
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
    if (errno == EINTR) {
      // Interrupted by signal, not an error
      return false;
    }
    
    LOG_ERROR("Select error: %s", strerror(errno));
    
    // Check if client socket is still valid
    if (errno == EBADF) {
      LOG_WARN("Client socket is no longer valid");
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
    LOG_ERROR("Receive error: %s", strerror(errno));
    return -1;
  }
  
  // Connection closed by client
  if (bytes_received == 0) {
    LOG_WARN("Connection closed by client");
    close(client_socket_fd_);
    client_socket_fd_ = -1;
    client_connected_ = false;
    return -1;
  }
  
  return bytes_received;
}

bool TcpServerTransport::receive_exact(void* buffer, size_t size)
{
    // If no client is connected, try to accept one with retries
    if (!client_connected_ || client_socket_fd_ < 0) {
        const int max_accept_attempts = 50; // e.g., 50 * 100ms = 5 seconds total wait time
        bool accepted_client = false;
        LOG_DEBUG("receive_exact: No client connected. Attempting to accept one (max %d attempts).", max_accept_attempts);
        for (int i = 0; i < max_accept_attempts; ++i) {
            if (!listening_) {
                LOG_WARN("receive_exact: Server is no longer listening. Cannot accept new clients.");
                return false; // Stop trying if server socket is closed
            }
            if (accept_connection()) { // accept_connection() sets client_connected_ and client_socket_fd_
                LOG_INFO("receive_exact: Client accepted on attempt %d.", i + 1);
                accepted_client = true;
                break; // Client connected, exit loop
            }
            // If accept_connection failed and we haven't reached max attempts, wait and retry
            if (i < max_accept_attempts - 1) {
                // LOG_DEBUG("receive_exact: No client on accept attempt %d/%d, sleeping for 100ms.", i + 1, max_accept_attempts);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        // After loop, check if a client was actually connected
        if (!accepted_client) {
            LOG_WARN("receive_exact: Failed to accept a client after %d attempts.", max_accept_attempts);
            return false; // Still no client, cannot receive
        }
    }

    // At this point, client_connected_ should be true and client_socket_fd_ should be valid.
    if (!client_connected_ || client_socket_fd_ < 0) {
        LOG_ERROR("receive_exact: Internal error - client should be connected but is not.");
        return false;
    }

    LOG_DEBUG("receive_exact: Attempting to receive %zu bytes from connected client (fd: %d).", size, client_socket_fd_);
    size_t total_received = 0;
    char* buf_ptr = static_cast<char*>(buffer);

    // Set a timeout for the receive operation itself, even after connection.
    // This uses select() for timeout on the client socket.
    fd_set read_fds;
    struct timeval tv;
    const int receive_timeout_seconds = 5; // Timeout for individual recv calls or overall receive phase

    while (total_received < size) {
        FD_ZERO(&read_fds);
        FD_SET(client_socket_fd_, &read_fds);

        // Set timeout for select()
        tv.tv_sec = receive_timeout_seconds;
        tv.tv_usec = 0;

        int activity = select(client_socket_fd_ + 1, &read_fds, nullptr, nullptr, &tv);

        if (activity < 0 && errno != EINTR) { // EINTR is an interrupt, can be retried
            LOG_ERROR("receive_exact: select() error: %s", strerror(errno));
            // Consider this a fatal error for this receive operation
            // The client connection might be compromised.
            // disconnect_client(); // Helper to close client_socket_fd_ and reset flags
            return false;
        }

        if (activity == 0) {
            // Timeout occurred
            LOG_ERROR("receive_exact: Timeout occurred waiting for data from client (waited %d seconds). Received %zu of %zu bytes.", receive_timeout_seconds, total_received, size);
            return false;
        }

        // If we are here, client_socket_fd_ is ready for reading
        ssize_t bytes_received = recv(client_socket_fd_,
                                     buf_ptr + total_received,
                                     size - total_received, 0); // MSG_WAITALL could be an option but select handles timeout better

        if (bytes_received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                // This shouldn't happen if select() indicated readability,
                // but handle defensively. It means no data available right now.
                // The select() timeout should prevent busy-looping here.
                LOG_DEBUG("receive_exact: recv() returned EWOULDBLOCK/EAGAIN despite select, retrying select.");
                continue; // Go back to select()
            }
            LOG_ERROR("receive_exact: recv() error: %s", strerror(errno));
            // disconnect_client(); // Helper to close client_socket_fd_ and reset flags
            return false;
        }

        if (bytes_received == 0) {
            // Connection closed by client
            LOG_WARN("receive_exact: Connection closed by client while receiving. Received %zu of %zu bytes.", total_received, size);
            // disconnect_client(); // Helper to close client_socket_fd_ and reset flags
            return false; // Return false as not all data was received
        }

        total_received += bytes_received;
        LOG_DEBUG("receive_exact: Received %zd bytes, total %zu/%zu.", bytes_received, total_received, size);
    }

    // This check is technically redundant if the loop condition is `total_received < size`,
    // but good for clarity or if loop logic changes.
    if (total_received < size) {
        LOG_ERROR("receive_exact: Failed to receive all data. Expected %zu, got %zu.", size, total_received);
        return false;
    }

    LOG_INFO("receive_exact: Successfully received %zu bytes.", size);
    return true;
}

} // namespace gateway