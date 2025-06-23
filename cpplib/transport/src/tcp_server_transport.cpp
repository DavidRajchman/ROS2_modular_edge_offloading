#include "transport/transport_base.hpp"
#include "transport/logging_utils.hpp"

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <sys/select.h>
#include <unordered_map>
#include <algorithm>

namespace gateway {

// Add this using declaration to fix the ClientId scope issue
using ClientId = TransportBase::ClientId;

TcpServerTransport::TcpServerTransport(int port, bool multi_client_mode, int max_clients)
  : port_(port),
    multi_client_mode_(multi_client_mode),
    max_clients_(max_clients),
    server_socket_fd_(-1),
    running_(false)
{
}

TcpServerTransport::~TcpServerTransport()
{
  disconnect();
}

bool TcpServerTransport::connect()
{
  // Close existing socket if already running
  if (running_) {
    disconnect();
  }
  
  try {
    // Create server socket
    server_socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd_ < 0) {
      LOG_ERROR("Failed to create socket: %s", strerror(errno));
      return false;
    }
    
    // Allow immediate address reuse
    int opt = 1;
    if (setsockopt(server_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
      LOG_ERROR("Failed to set socket options: %s", strerror(errno));
      close(server_socket_fd_);
      server_socket_fd_ = -1;
      return false;
    }
    
    // Bind socket to port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);
    
    if (bind(server_socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
      LOG_ERROR("Failed to bind socket: %s", strerror(errno));
      close(server_socket_fd_);
      server_socket_fd_ = -1;
      return false;
    }
    
    // Set socket to non-blocking mode
    int flags = fcntl(server_socket_fd_, F_GETFL, 0);
    if (flags < 0 || fcntl(server_socket_fd_, F_SETFL, flags | O_NONBLOCK) < 0) {
      LOG_ERROR("Failed to set socket non-blocking: %s", strerror(errno));
      close(server_socket_fd_);
      server_socket_fd_ = -1;
      return false;
    }
    
    // Start listening for connections
    if (listen(server_socket_fd_, multi_client_mode_ ? (max_clients_ ? max_clients_ : SOMAXCONN) : 1) < 0) {
      LOG_ERROR("Failed to listen on socket: %s", strerror(errno));
      close(server_socket_fd_);
      server_socket_fd_ = -1;
      return false;
    }
    
    running_ = true;
    
    // Log running information
    struct sockaddr_in actual_addr;
    socklen_t addr_len = sizeof(actual_addr);
    if (getsockname(server_socket_fd_, (struct sockaddr*)&actual_addr, &addr_len) == 0) {
      char host[INET_ADDRSTRLEN];
      inet_ntop(AF_INET, &actual_addr.sin_addr, host, INET_ADDRSTRLEN);
      LOG_INFO("TCP server listening on %s:%d in %s mode", 
               host, ntohs(actual_addr.sin_port), 
               multi_client_mode_ ? "multi-client" : "single-client");
    } else {
      LOG_INFO("TCP server listening on port %d in %s mode", 
               port_, multi_client_mode_ ? "multi-client" : "single-client");
    }
    
    return true;
  }
  catch (const std::exception& ex) {
    if (server_socket_fd_ >= 0) {
      close(server_socket_fd_);
      server_socket_fd_ = -1;
    }
    running_ = false;
    LOG_ERROR("Failed to start server: %s", ex.what());
    return false;
  }
}

void TcpServerTransport::disconnect()
{
  // Use the running_ flag as a guard to prevent re-entrant calls.
  if (!running_) {
    return;
  }
  running_ = false; // Mark that we are now shutting down.

  // Close all client connections safely.
  // First, make a copy of the client IDs to iterate over. This is crucial because
  // the disconnect_client call below will trigger a callback that modifies the
  // original clients_ map, which would invalidate iterators.
  std::vector<ClientId> client_ids_to_disconnect;
  client_ids_to_disconnect.reserve(clients_.size());
  for (const auto& client_pair : clients_) {
    client_ids_to_disconnect.push_back(client_pair.first);
  }

  // Now, disconnect each client using the copied list.
  for (const auto& id : client_ids_to_disconnect) {
      disconnect_client(id);
  }
  
  // Close the main server socket
  if (server_socket_fd_ >= 0) {
    close(server_socket_fd_);
    server_socket_fd_ = -1;
  }
}


bool TcpServerTransport::is_connected() const
{
  // In single client mode, we are "connected" if there is one client
  if (!multi_client_mode_) {
    return !clients_.empty();
  }
  
  // In multi-client mode, we are "connected" if the server is running
  return running_;
}

bool TcpServerTransport::process_events(int timeout_ms)
{
  if (!running_ || server_socket_fd_ < 0) {
    return false;
  }
  
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(server_socket_fd_, &read_fds);
  
  int max_fd = server_socket_fd_;
  
  // Add client sockets
  for (const auto& client : clients_) {
    FD_SET(client.second.socket_fd, &read_fds);
    max_fd = std::max(max_fd, client.second.socket_fd);
  }
  
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  
  int activity = select(max_fd + 1, &read_fds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL);
  
  if (activity < 0) {
    if (errno == EINTR) {
      // Interrupted by signal, just continue
      return true;
    }
    LOG_ERROR("select() error: %s", strerror(errno));
    return false;
  }
  
  if (activity == 0) {
    // Timeout, no activity
    return true;
  }
  
  // Check for new connections
  if (FD_ISSET(server_socket_fd_, &read_fds)) {
    accept_new_connections();
  }
  
  // Check client sockets - copy keys to avoid invalidation during iteration
  std::vector<ClientId> client_ids;
  for (const auto& client : clients_) {
    client_ids.push_back(client.first);
  }
  
  for (auto client_id : client_ids) {
    // Client may have been removed during a previous iteration
    auto it = clients_.find(client_id);
    if (it == clients_.end()) {
      continue;
    }
    
    if (FD_ISSET(it->second.socket_fd, &read_fds)) {
      // Data available or connection closed
      handle_client_data(client_id);
    }
  }
  
  return true;
}

bool TcpServerTransport::accept_new_connections()
{
  struct sockaddr_in client_addr;
  socklen_t client_len = sizeof(client_addr);
  
  int client_fd = accept(server_socket_fd_, (struct sockaddr*)&client_addr, &client_len);
  
  if (client_fd < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // No pending connections
      return true;
    }
    LOG_ERROR("accept() error: %s", strerror(errno));
    return false;
  }
  
  // Set client socket to non-blocking mode
  int flags = fcntl(client_fd, F_GETFL, 0);
  if (flags < 0 || fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
    LOG_ERROR("Failed to set client socket non-blocking: %s", strerror(errno));
    close(client_fd);
    return false;
  }
  
  // In single-client mode, disconnect any existing client first
  if (!multi_client_mode_ && !clients_.empty()) {
    LOG_INFO("New client connecting in single-client mode, disconnecting existing client");
    
    // Disconnect all existing clients (should just be one)
    for (const auto& client_pair : clients_) {
      if (disconnect_callback_) {
        disconnect_callback_(client_pair.first);
      }
      close(client_pair.second.socket_fd);
    }
    clients_.clear();
  }
  
  // Enforce max clients limit in multi-client mode
  if (multi_client_mode_ && max_clients_ > 0 && clients_.size() >= static_cast<size_t>(max_clients_)) {
    LOG_WARN("Max clients limit (%d) reached, rejecting new connection", max_clients_);
    close(client_fd);
    return false;
  }
  
  // Create client info
  ClientInfo client_info;
  client_info.socket_fd = client_fd;
  client_info.ip_address = inet_ntoa(client_addr.sin_addr);
  client_info.port = ntohs(client_addr.sin_port);
  
  // In single-client mode, always use ClientId 0
  // In multi-client mode, use the socket fd as the client ID
  ClientId client_id = multi_client_mode_ ? client_fd : DEFAULT_CLIENT;
  
  // Store client info
  clients_[client_id] = std::move(client_info);
  
  LOG_INFO("New client connected from %s:%d, assigned ID %d", 
           clients_[client_id].ip_address.c_str(), 
           clients_[client_id].port, 
           client_id);
  
  // Notify of new connection
  if (connect_callback_) {
    connect_callback_(client_id, clients_[client_id].ip_address, clients_[client_id].port);
  }
  
  return true;
}

void TcpServerTransport::handle_client_data(ClientId client_id)
{
  auto it = clients_.find(client_id);
  if (it == clients_.end()) {
    return; // Client not found
  }
  
  // Check if the client has data or has disconnected
  char buffer[1];
  ssize_t bytes_read = recv(it->second.socket_fd, buffer, sizeof(buffer), MSG_PEEK);
  
  if (bytes_read <= 0) {
    if (bytes_read == 0 || errno != EAGAIN) {
      // Client disconnected or error
      LOG_INFO("Client %d disconnected", client_id);
      if (disconnect_callback_) {
        disconnect_callback_(client_id);
      }
      close(it->second.socket_fd);
      clients_.erase(it);
    }
    return;
  }
  
  // Notify that data is available
  if (data_callback_) {
    data_callback_(client_id);
  }
}

bool TcpServerTransport::send_data(const void* data, size_t size)
{
  // In single-client mode, this sends to DEFAULT_CLIENT
  // In multi-client mode, it sends to the first client (if any)
  if (clients_.empty()) {
    LOG_ERROR("Cannot send data: no clients connected");
    return false;
  }
  
  ClientId target_client;
  if (!multi_client_mode_) {
    // In single-client mode, use DEFAULT_CLIENT
    target_client = DEFAULT_CLIENT;
  } else {
    // In multi-client mode, use the first client
    target_client = clients_.begin()->first;
  }
  
  return send_to(target_client, data, size);
}

int TcpServerTransport::receive_data(void* buffer, size_t max_size)
{
  // Similar logic to send_data
  if (clients_.empty()) {
    return -1;
  }
  
  ClientId source_client;
  if (!multi_client_mode_) {
    // In single-client mode, use DEFAULT_CLIENT
    source_client = DEFAULT_CLIENT;
  } else {
    // In multi-client mode, use the first client
    source_client = clients_.begin()->first;
  }
  
  return receive_from(source_client, buffer, max_size);
}

bool TcpServerTransport::data_available(int timeout_ms)
{
  // Similar logic to send_data and receive_data
  if (clients_.empty()) {
    return false;
  }
  
  ClientId check_client;
  if (!multi_client_mode_) {
    // In single-client mode, use DEFAULT_CLIENT
    check_client = DEFAULT_CLIENT;
  } else {
    // In multi-client mode, use the first client
    check_client = clients_.begin()->first;
  }
  
  return data_available_from(check_client, timeout_ms);
}

bool TcpServerTransport::receive_exact(void* buffer, size_t size)
{
  if (size == 0) {
    return true;
  }
  
  uint8_t* buf = static_cast<uint8_t*>(buffer);
  size_t received = 0;
  
  while (received < size) {
    int bytes = receive_data(buf + received, size - received);
    if (bytes <= 0) {
      return false;
    }
    received += bytes;
  }
  
  return true;
}

bool TcpServerTransport::send_to(ClientId client_id, const void* data, size_t size)
{
  auto it = clients_.find(client_id);
  if (it == clients_.end()) {
    LOG_ERROR("Cannot send to client %d: not connected", client_id);
    return false;
  }
  
  const uint8_t* buf = static_cast<const uint8_t*>(data);
  size_t total_sent = 0;
  
  while (total_sent < size) {
    ssize_t sent = send(it->second.socket_fd, buf + total_sent, size - total_sent, 0);
    
    if (sent <= 0) {
      if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        // Would block, try again later
        fd_set write_fds;
        FD_ZERO(&write_fds);
        FD_SET(it->second.socket_fd, &write_fds);
        
        // Wait for socket to become writable
        struct timeval tv;
        tv.tv_sec = 1;  // 1 second timeout
        tv.tv_usec = 0;
        
        if (select(it->second.socket_fd + 1, NULL, &write_fds, NULL, &tv) <= 0) {
          LOG_ERROR("Failed to send to client %d: socket not writable", client_id);
          return false;
        }
        continue;
      } else {
        LOG_ERROR("Failed to send to client %d: %s", client_id, strerror(errno));
        return false;
      }
    }
    
    total_sent += sent;
  }
  
  return true;
}

int TcpServerTransport::receive_from(ClientId client_id, void* buffer, size_t max_size)
{
  auto it = clients_.find(client_id);
  if (it == clients_.end()) {
    return -1;
  }
  
  ssize_t bytes_read = recv(it->second.socket_fd, buffer, max_size, 0);
  
  if (bytes_read <= 0) {
    if (bytes_read == 0) {
      // Client disconnected
      LOG_INFO("Client %d disconnected during receive", client_id);
      if (disconnect_callback_) {
        disconnect_callback_(client_id);
      }
      close(it->second.socket_fd);
      clients_.erase(it);
      return 0;
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
      // No data available
      return 0;
    } else {
      // Error
      LOG_ERROR("Error receiving from client %d: %s", client_id, strerror(errno));
      return -1;
    }
  }
  
  return bytes_read;
}

bool TcpServerTransport::data_available_from(ClientId client_id, int timeout_ms)
{
  auto it = clients_.find(client_id);
  if (it == clients_.end()) {
    return false;
  }
  
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(it->second.socket_fd, &read_fds);
  
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  
  int result = select(it->second.socket_fd + 1, &read_fds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL);
  
  if (result < 0) {
    LOG_ERROR("select() error in data_available_from: %s", strerror(errno));
    return false;
  }
  
  if (result == 0) {
    // Timeout
    return false;
  }
  
  // Check if the client has actually disconnected
  char buffer[1];
  ssize_t bytes_read = recv(it->second.socket_fd, buffer, sizeof(buffer), MSG_PEEK);
  
  if (bytes_read <= 0 && (bytes_read == 0 || errno != EAGAIN)) {
    // Client disconnected
    LOG_INFO("Client %d disconnected during data_available check", client_id);
    if (disconnect_callback_) {
      disconnect_callback_(client_id);
    }
    close(it->second.socket_fd);
    clients_.erase(it);
    return false;
  }
  
  return (bytes_read > 0);
}

int TcpServerTransport::broadcast(const void* data, size_t size)
{
  int success_count = 0;
  
  for (const auto& client : clients_) {
    if (send_to(client.first, data, size)) {
      success_count++;
    }
  }
  
  return success_count;
}

// Fixed: Properly qualify ClientId in the function signature
std::vector<ClientId> TcpServerTransport::get_client_ids() const
{
  std::vector<ClientId> client_ids;
  client_ids.reserve(clients_.size());
  
  for (const auto& client : clients_) {
    client_ids.push_back(client.first);
  }
  
  return client_ids;
}

bool TcpServerTransport::is_client_connected(ClientId client_id) const
{
  return clients_.find(client_id) != clients_.end();
}

void TcpServerTransport::disconnect_client(ClientId client_id)
{
  auto it = clients_.find(client_id);
  if (it == clients_.end()) {
    return;
  }
  
  LOG_INFO("Disconnecting client %d", client_id);
  
  if (disconnect_callback_) {
    disconnect_callback_(client_id);
  }
  
  close(it->second.socket_fd);
  clients_.erase(it);
}

void TcpServerTransport::set_connect_callback(ConnectCallback callback)
{
  connect_callback_ = callback;
}

void TcpServerTransport::set_disconnect_callback(DisconnectCallback callback)
{
  disconnect_callback_ = callback;
}

void TcpServerTransport::set_data_callback(DataCallback callback)
{
  data_callback_ = callback;
}

} // namespace gateway