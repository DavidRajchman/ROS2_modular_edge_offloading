#ifndef TRANSPORT_BASE_HPP
#define TRANSPORT_BASE_HPP

#include <memory>
#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include "readerwriterqueue.h" 

namespace gateway {

enum class TransportAsyncSendResult {
  SUCCESS,
  QUEUE_FULL,
  NOT_CONNECTED,
  INTERNAL_ERROR
};

class TransportBase {
public:
  virtual ~TransportBase() = default;

  virtual bool connect() = 0;
  virtual void disconnect() = 0;

  // Async send interface
  virtual TransportAsyncSendResult async_send_data(std::vector<uint8_t>&& data) = 0;

  virtual bool is_connected() const = 0;

  virtual bool data_available(int timeout_ms = 0) = 0;
  virtual int receive_data(void* buffer, size_t max_size) = 0;
  virtual bool receive_exact(void* buffer, size_t size) = 0;
};

class TcpServerTransport : public TransportBase {
public:
  TcpServerTransport(int port, int max_connections = 1);
  virtual ~TcpServerTransport();

  bool connect() override;
  void disconnect() override;
  bool is_connected() const override;
  TransportAsyncSendResult async_send_data(std::vector<uint8_t>&& data) override;
  bool data_available(int timeout_ms = 0) override;
  int receive_data(void* buffer, size_t max_size) override;
  bool receive_exact(void* buffer, size_t size) override;

private:
  int port_;
  int max_connections_;
  std::atomic<int> server_socket_fd_;
  std::atomic<int> client_socket_fd_;
  std::atomic<bool> listening_;
  std::atomic<bool> client_connected_;
  std::atomic<uint64_t> connection_timestamp_;
  std::atomic<bool> is_reconnecting_;
  std::atomic<int> io_operation_active_count_;
  rclcpp::Logger logger_{rclcpp::get_logger("tcp_server_transport")};

  moodycamel::ReaderWriterQueue<std::vector<uint8_t>> outgoing_queue_;
  std::atomic<bool> sender_thread_running_;
  std::thread sender_thread_;

  bool accept_connection();
  void sender_thread_func();
  void handle_disconnect_detected();
};


class TcpClientTransport : public TransportBase {
public:
  TcpClientTransport(const std::string& host, int port, int max_retries = 3);
  virtual ~TcpClientTransport();

  bool connect() override;
  void disconnect() override;
  bool is_connected() const override;
  TransportAsyncSendResult async_send_data(std::vector<uint8_t>&& data) override;
  bool data_available(int timeout_ms = 0) override;
  int receive_data(void* buffer, size_t max_size) override;
  bool receive_exact(void* buffer, size_t size) override;

private:
  std::string server_host_;
  int server_port_;
  int max_retries_;
  
  std::atomic<int> socket_fd_;
  std::atomic<bool> connected_;
  std::atomic<uint64_t> connection_timestamp_;
  std::atomic<bool> is_reconnecting_;
  
  rclcpp::Logger logger_{rclcpp::get_logger("tcp_client_transport")};

  moodycamel::ReaderWriterQueue<std::vector<uint8_t>> outgoing_queue_;
  std::atomic<bool> sender_thread_running_;
  std::thread sender_thread_;

  void sender_thread_func();
  void handle_disconnect_detected();
  bool perform_connect_logic();
};


} // namespace gateway
#endif // TRANSPORT_BASE_HPP