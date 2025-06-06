#ifndef TRANSPORT_BASE_HPP
#define TRANSPORT_BASE_HPP

#include <cstddef>
#include <vector>
#include <string>
#include <functional>

namespace gateway {

/**
 * Base class for all transport implementations
 */
class TransportBase {
public:
  using ClientId = int;
  using ConnectCallback = std::function<void(ClientId, const std::string&, int)>;
  using DisconnectCallback = std::function<void(ClientId)>;
  using DataCallback = std::function<void(ClientId)>;

  static const ClientId DEFAULT_CLIENT = 0;

  virtual ~TransportBase() = default;
  
  // Connect/disconnect for client connections or start/stop for servers
  virtual bool connect() = 0;
  virtual void disconnect() = 0;
  
  // Check connection state
  virtual bool is_connected() const = 0;

  // Process events and accept new connections (for server implementations)
  virtual bool process_events(int timeout_ms = 0) = 0;
  
  // Single-client mode operations (also works with DEFAULT_CLIENT for multi-client)
  virtual bool send_data(const void* data, size_t size) = 0;
  virtual int receive_data(void* buffer, size_t max_size) = 0;  
  virtual bool data_available(int timeout_ms = 0) = 0;
  virtual bool receive_exact(void* buffer, size_t size) = 0;
  
  // Multi-client mode operations
  virtual bool send_to(ClientId client_id, const void* data, size_t size) = 0;
  virtual int receive_from(ClientId client_id, void* buffer, size_t max_size) = 0;
  virtual bool data_available_from(ClientId client_id, int timeout_ms = 0) = 0;
  
  // Broadcast to all connected clients (for server implementations)
  virtual int broadcast(const void* data, size_t size) = 0;
  
  // Client management (for server implementations)
  virtual std::vector<ClientId> get_client_ids() const = 0;
  virtual bool is_client_connected(ClientId client_id) const = 0;
  virtual void disconnect_client(ClientId client_id) = 0;
  
  // Client events
  virtual void set_connect_callback(ConnectCallback callback) = 0;
  virtual void set_disconnect_callback(DisconnectCallback callback) = 0;
  virtual void set_data_callback(DataCallback callback) = 0;

  // Multi-client mode control
  virtual bool is_multi_client_mode() const = 0;
};

/**
 * TCP Client Transport implementation
 */
class TcpClientTransport : public TransportBase {
public:
  TcpClientTransport(const std::string& host, int port, int max_retries = 3);
  virtual ~TcpClientTransport();
  
  // TransportBase implementation
  bool connect() override;
  void disconnect() override;
  bool is_connected() const override;
  bool process_events(int timeout_ms = 0) override;
  
  bool send_data(const void* data, size_t size) override;
  int receive_data(void* buffer, size_t max_size) override;
  bool data_available(int timeout_ms = 0) override;
  bool receive_exact(void* buffer, size_t size) override;
  
  // Multi-client operations (not applicable for client, but must implement)
  bool send_to(ClientId client_id, const void* data, size_t size) override;
  int receive_from(ClientId client_id, void* buffer, size_t max_size) override;
  bool data_available_from(ClientId client_id, int timeout_ms = 0) override;
  int broadcast(const void* data, size_t size) override;
  
  std::vector<ClientId> get_client_ids() const override;
  bool is_client_connected(ClientId client_id) const override;
  void disconnect_client(ClientId client_id) override;
  
  void set_connect_callback(ConnectCallback callback) override;
  void set_disconnect_callback(DisconnectCallback callback) override;
  void set_data_callback(DataCallback callback) override;
  
  bool is_multi_client_mode() const override { return false; }
  
private:
  std::string host_;
  int port_;
  int max_retries_;
  int socket_fd_;
  bool connected_;
  
  ConnectCallback connect_callback_;
  DisconnectCallback disconnect_callback_;
  DataCallback data_callback_;
};

/**
 * TCP Server Transport implementation
 * Can operate in single-client or multi-client mode
 */
class TcpServerTransport : public TransportBase {
public:
  // The multi_client_mode parameter controls whether:
  // - false (default): Only one client can be connected at a time (old behavior)
  // - true: Multiple clients can be connected simultaneously
  TcpServerTransport(int port, bool multi_client_mode = false, int max_clients = 0);
  virtual ~TcpServerTransport();
  
  // TransportBase implementation
  bool connect() override;
  void disconnect() override;
  bool is_connected() const override;
  bool process_events(int timeout_ms = 0) override;
  
  // Single client mode (uses the first/only client)
  bool send_data(const void* data, size_t size) override;
  int receive_data(void* buffer, size_t max_size) override;
  bool data_available(int timeout_ms = 0) override;
  bool receive_exact(void* buffer, size_t size) override;
  
  // Multi-client operations
  bool send_to(ClientId client_id, const void* data, size_t size) override;
  int receive_from(ClientId client_id, void* buffer, size_t max_size) override;
  bool data_available_from(ClientId client_id, int timeout_ms = 0) override;
  int broadcast(const void* data, size_t size) override;
  
  std::vector<ClientId> get_client_ids() const override;
  bool is_client_connected(ClientId client_id) const override;
  void disconnect_client(ClientId client_id) override;
  
  void set_connect_callback(ConnectCallback callback) override;
  void set_disconnect_callback(DisconnectCallback callback) override;
  void set_data_callback(DataCallback callback) override;
  
  bool is_multi_client_mode() const override { return multi_client_mode_; }
  
private:
  int port_;
  bool multi_client_mode_;
  int max_clients_;
  int server_socket_fd_;
  bool running_;
  
  struct ClientInfo {
    int socket_fd;
    std::string ip_address;
    int port;
    
    ClientInfo() : socket_fd(-1), port(0) {}
  };
  
  // In single-client mode, we only use the first entry
  std::unordered_map<ClientId, ClientInfo> clients_;
  
  ConnectCallback connect_callback_;
  DisconnectCallback disconnect_callback_;
  DataCallback data_callback_;
  
  bool accept_new_connections();
  void handle_client_data(ClientId client_id);
};

} // namespace gateway

#endif // TRANSPORT_BASE_HPP