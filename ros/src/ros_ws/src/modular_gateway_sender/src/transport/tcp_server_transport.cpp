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

TcpServerTransport::TcpServerTransport(int port, int max_connections)
  : port_(port),
    max_connections_(max_connections),
    server_socket_fd_(-1),
    client_socket_fd_(-1),
    listening_(false),
    client_connected_(false),
    connection_timestamp_(0),
    is_reconnecting_(false),
    io_operation_active_count_(0),
    outgoing_queue_(1024), // SPSC queue size
    sender_thread_running_(true)
{
    sender_thread_ = std::thread(&TcpServerTransport::sender_thread_func, this);
}

TcpServerTransport::~TcpServerTransport()
{
    sender_thread_running_ = false;
    if (sender_thread_.joinable()) sender_thread_.join();
    disconnect();
}

bool TcpServerTransport::connect()
{
    disconnect();
    try {
        int new_socket_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (new_socket_fd < 0) {
            throw std::runtime_error(std::string("Failed to create socket: ") + strerror(errno));
        }
        server_socket_fd_ = new_socket_fd;

        int opt = 1;
        if (::setsockopt(server_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            throw std::runtime_error(std::string("Failed to set socket options: ") + strerror(errno));
        }
        if (::setsockopt(server_socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt)) < 0) {
            LOG_WARN(logger_, "Failed to set keepalive: %s", strerror(errno));
        }
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port_);
        if (::bind(server_socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            throw std::runtime_error(std::string("Bind failed: ") + strerror(errno));
        }
        if (::listen(server_socket_fd_, max_connections_) < 0) {
            throw std::runtime_error(std::string("Listen failed: ") + strerror(errno));
        }
        int flags = ::fcntl(server_socket_fd_, F_GETFL, 0);
        if (flags == -1 || ::fcntl(server_socket_fd_, F_SETFL, flags | O_NONBLOCK) == -1) {
            throw std::runtime_error(std::string("Failed to set server socket non-blocking: ") + strerror(errno));
        }
        listening_ = true;
        client_connected_ = false;
        connection_timestamp_++;
        LOG_INFO(logger_, "TCP server listening on port %d", port_);
        return true;
    } catch (const std::exception& ex) {
        if (server_socket_fd_ >= 0) {
            ::close(server_socket_fd_);
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
    listening_ = false;
    client_connected_ = false;
    int client_fd = client_socket_fd_.exchange(-1);
    if (client_fd >= 0) {
        ::close(client_fd);
        LOG_INFO(logger_, "Client connection closed");
    }
    int server_fd = server_socket_fd_.exchange(-1);
    if (server_fd >= 0) {
        ::close(server_fd);
        LOG_INFO(logger_, "Server stopped listening");
    }
}

bool TcpServerTransport::is_connected() const
{
    return client_connected_.load();
}

TransportAsyncSendResult TcpServerTransport::async_send_data(std::vector<uint8_t>&& data)
{
    if (!is_connected()) return TransportAsyncSendResult::NOT_CONNECTED;
    if (!outgoing_queue_.try_enqueue(std::move(data))) {
        return TransportAsyncSendResult::QUEUE_FULL;
    }
    return TransportAsyncSendResult::SUCCESS;
}

void TcpServerTransport::sender_thread_func()
{
    while (sender_thread_running_) {
        std::vector<uint8_t> data;
        if (!outgoing_queue_.try_dequeue(data)) {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            continue;
        }
        if (!is_connected()) continue;
        int fd = client_socket_fd_.load();
        if (fd < 0) continue;
        size_t offset = 0;
        while (offset < data.size()) {
            ssize_t sent = ::send(fd, data.data() + offset, data.size() - offset, MSG_NOSIGNAL);
            if (sent < 0) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    handle_disconnect_detected();
                }
                break;
            }
            offset += sent;
        }
    }
}

void TcpServerTransport::handle_disconnect_detected()
{
    if(client_connected_.exchange(false)) {
        int client_fd = client_socket_fd_.exchange(-1);
        if (client_fd >= 0) {
            ::close(client_fd);
        }
        connection_timestamp_++;
        LOG_WARN(logger_, "Client disconnect detected and handled.");
    }
}

bool TcpServerTransport::accept_connection()
{
    if (!listening_ || server_socket_fd_ < 0) {
        return false;
    }
    if (client_connected_ && client_socket_fd_ >= 0) {
        if (::send(client_socket_fd_, nullptr, 0, MSG_NOSIGNAL) < 0) {
            if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN || errno == EBADF) {
                LOG_WARN(logger_, "Client disconnected (checked via send): %s", strerror(errno));
                handle_disconnect_detected();
            }
        } else {
            return true;
        }
    }
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int new_socket = ::accept(server_socket_fd_, (struct sockaddr*)&client_addr, &client_len);
    if (new_socket < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR(logger_, "Accept failed: %s", strerror(errno));
        }
        return false;
    }
    client_socket_fd_ = new_socket;
    client_connected_ = true;
    connection_timestamp_++;
    int opt = 1;
    ::setsockopt(client_socket_fd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    ::setsockopt(client_socket_fd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
    int flags = ::fcntl(client_socket_fd_, F_GETFL, 0);
    if (flags != -1) {
        ::fcntl(client_socket_fd_, F_SETFL, flags | O_NONBLOCK);
    }
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    LOG_INFO(logger_, "Client connected from %s:%d", client_ip, ntohs(client_addr.sin_port));
    return true;
}

bool TcpServerTransport::data_available(int timeout_ms)
{
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
    int result = ::select(client_socket_fd_ + 1, &readfds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL);
    if (result < 0) {
        LOG_ERROR(logger_, "Select error in data_available: %s", strerror(errno));
        if (errno == EBADF) {
            handle_disconnect_detected();
        }
        return false;
    }
    return result > 0;
}

int TcpServerTransport::receive_data(void* buffer, size_t max_size)
{
    accept_connection();
    if (!client_connected_ || client_socket_fd_ < 0) {
        return -1;
    }
    ssize_t bytes_received = ::recv(client_socket_fd_, buffer, max_size, 0);
    if (bytes_received < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return 0;
        }
        LOG_ERROR(logger_, "Receive error: %s", strerror(errno));
        handle_disconnect_detected();
        return -1;
    }
    if (bytes_received == 0) {
        LOG_WARN(logger_, "Connection closed by peer (recv 0)");
        handle_disconnect_detected();
        return -1;
    }
    return bytes_received;
}

bool TcpServerTransport::receive_exact(void* buffer, size_t size)
{
    accept_connection();
    if (!client_connected_ || client_socket_fd_ < 0) {
        return false;
    }
    size_t total_received = 0;
    char* buf_ptr = static_cast<char*>(buffer);
    while (total_received < size) {
        ssize_t bytes_received = ::recv(client_socket_fd_, buf_ptr + total_received, size - total_received, 0);
        if (bytes_received < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                continue;
            }
            LOG_ERROR(logger_, "Receive error in receive_exact: %s", strerror(errno));
            handle_disconnect_detected();
            return false;
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