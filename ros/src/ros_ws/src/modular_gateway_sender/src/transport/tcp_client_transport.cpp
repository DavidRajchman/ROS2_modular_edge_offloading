#include "modular_gateway_sender/logging_utils.hpp"
#include "modular_gateway_sender/transport_base.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
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
    connected_(false),
    connection_timestamp_(0),
    is_reconnecting_(false),
    outgoing_queue_(1024), // SPSC queue size
    sender_thread_running_(true)
{
    sender_thread_ = std::thread(&TcpClientTransport::sender_thread_func, this);
}

TcpClientTransport::~TcpClientTransport()
{
    sender_thread_running_ = false;
    if (sender_thread_.joinable()) sender_thread_.join();
    disconnect();
}

bool TcpClientTransport::perform_connect_logic() {
    bool expected_reconnecting = false;
    if (!is_reconnecting_.compare_exchange_strong(expected_reconnecting, true)) {
        LOG_WARN(logger_, "Connection attempt already in progress.");
        return is_connected();
    }

    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;

    try {
        int new_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (new_socket_fd < 0) {
            throw std::runtime_error(std::string("Failed to create socket: ") + strerror(errno));
        }

        int opt = 1;
        setsockopt(new_socket_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        setsockopt(new_socket_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port_);
        if (inet_pton(AF_INET, server_host_.c_str(), &server_addr.sin_addr) <= 0) {
            throw std::runtime_error(std::string("Invalid address: ") + server_host_);
        }

        if (::connect(new_socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            throw std::runtime_error(std::string("Connection failed: ") + strerror(errno));
        }

        int flags = fcntl(new_socket_fd, F_GETFL, 0);
        if (flags != -1) {
            fcntl(new_socket_fd, F_SETFL, flags | O_NONBLOCK);
        }

        socket_fd_ = new_socket_fd;
        connected_ = true;
        connection_timestamp_++;
        LOG_INFO(logger_, "Successfully connected to %s:%d", server_host_.c_str(), server_port_);
        is_reconnecting_ = false;
        return true;
    } catch (const std::exception& ex) {
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
        connected_ = false;
        LOG_ERROR(logger_, "Connection to %s:%d failed: %s", server_host_.c_str(), server_port_, ex.what());
        is_reconnecting_ = false;
        return false;
    }
}

bool TcpClientTransport::connect()
{
    return perform_connect_logic();
}

void TcpClientTransport::disconnect()
{
    connected_ = false;
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
        LOG_INFO(logger_, "Disconnected from server.");
    }
}

TransportAsyncSendResult TcpClientTransport::async_send_data(std::vector<uint8_t>&& data)
{
    if (!is_connected()) return TransportAsyncSendResult::NOT_CONNECTED;
    if (!outgoing_queue_.try_enqueue(std::move(data))) {
        return TransportAsyncSendResult::QUEUE_FULL;
    }
    return TransportAsyncSendResult::SUCCESS;
}

void TcpClientTransport::sender_thread_func()
{
    while (sender_thread_running_) {
        if (!is_connected()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        std::vector<uint8_t> data;
        if (!outgoing_queue_.try_dequeue(data)) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            continue;
        }

        int fd = socket_fd_.load();
        uint64_t timestamp_before_send = connection_timestamp_.load();
        size_t offset = 0;
        bool send_error = false;

        while (offset < data.size()) {
            ssize_t sent = ::send(fd, data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
            if (sent < 0) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    LOG_ERROR(logger_, "Send failed: %s", strerror(errno));
                    handle_disconnect_detected();
                    send_error = true;
                }
                break;
            }
            offset += sent;
        }

        if (!send_error && timestamp_before_send != connection_timestamp_.load()) {
            LOG_ERROR(logger_, "CRITICAL FAULT: Data sent successfully, but connection instance changed. Data delivery uncertain.");
        }
    }
}

void TcpClientTransport::handle_disconnect_detected()
{
    if (is_connected()) {
        LOG_WARN(logger_, "Disconnect detected.");
        disconnect();
    }
}

bool TcpClientTransport::is_connected() const
{
    return connected_.load();
}

bool TcpClientTransport::data_available(int timeout_ms) {
    if (!is_connected()) return false;
    
    fd_set readfds;
    struct timeval tv;
    
    FD_ZERO(&readfds);
    int fd = socket_fd_.load();
    if (fd < 0) return false;
    FD_SET(fd, &readfds);
    
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    int result = select(fd + 1, &readfds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL);
    
    if (result < 0) {
        LOG_ERROR(logger_, "Select error: %s", strerror(errno));
        if (errno == EBADF) handle_disconnect_detected();
        return false;
    }
    
    return result > 0;
}

int TcpClientTransport::receive_data(void* buffer, size_t size) {
    if (!is_connected()) return -1;
    
    int fd = socket_fd_.load();
    if (fd < 0) return -1;
    ssize_t bytes_received = recv(fd, buffer, size, 0);
    
    if (bytes_received < 0) {
        if (errno != EWOULDBLOCK && errno != EAGAIN) {
            LOG_ERROR(logger_, "Receive error: %s", strerror(errno));
            handle_disconnect_detected();
        }
        return -1;
    }
    
    if (bytes_received == 0) {
        LOG_WARN(logger_, "Connection closed by peer");
        handle_disconnect_detected();
        return -1;
    }
    
    return bytes_received;
}

bool TcpClientTransport::receive_exact(void* buffer, size_t size) {
    if (!is_connected()) return false;
    
    size_t total_received = 0;
    char* buf_ptr = static_cast<char*>(buffer);
    int fd = socket_fd_.load();
    if (fd < 0) return false;
    
    while (total_received < size) {
        ssize_t bytes_received = recv(fd, buf_ptr + total_received, size - total_received, 0);
        
        if (bytes_received < 0) {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                LOG_ERROR(logger_, "Receive error in receive_exact: %s", strerror(errno));
                handle_disconnect_detected();
                return false;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            continue;
        }
        
        if (bytes_received == 0) {
            LOG_WARN(logger_, "Connection closed by peer during receive_exact");
            handle_disconnect_detected();
            return false;
        }
        
        total_received += bytes_received;
    }
    
    return true;
}

} // namespace gateway