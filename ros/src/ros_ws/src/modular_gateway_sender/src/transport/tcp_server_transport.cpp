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
      logger_(CppLogging::Logger("gateway")), // Initialize correct logger
      sender_thread_running_(false)
{
}


TcpServerTransport::~TcpServerTransport()
{
    disconnect();
}
bool TcpServerTransport::connect()
{
    if (listening_)
    {
        logger_.Warn("tcp_server_transport.cpp: Already listening.");
        return true;
    }

    server_socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd_ < 0)
    {
        logger_.Error("tcp_server_transport.cpp: Failed to create server socket: {}", strerror(errno));
        return false;
    }

    int opt = 1;
    if (::setsockopt(server_socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        logger_.Error("tcp_server_transport.cpp: setsockopt(SO_REUSEADDR) failed: {}", strerror(errno));
        ::close(server_socket_fd_);
        server_socket_fd_ = -1;
        return false;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (::bind(server_socket_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        logger_.Error("tcp_server_transport.cpp: Failed to bind server socket: {}", strerror(errno));
        ::close(server_socket_fd_);
        server_socket_fd_ = -1;
        return false;
    }

    if (::listen(server_socket_fd_, max_connections_) < 0)
    {
        logger_.Error("tcp_server_transport.cpp: Failed to listen on server socket: {}", strerror(errno));
        ::close(server_socket_fd_);
        server_socket_fd_ = -1;
        return false;
    }

    listening_ = true;
    logger_.Info("tcp_server_transport.cpp: Server listening on port {}", port_);
    return true;
}


void TcpServerTransport::disconnect()
{
    sender_thread_running_ = false;
    if (sender_thread_.joinable())
    {
        sender_thread_.join();
    }

    if (client_socket_fd_ >= 0)
    {
        ::close(client_socket_fd_);
        client_socket_fd_ = -1;
    }
    client_connected_ = false;

    if (server_socket_fd_ >= 0)
    {
        ::close(server_socket_fd_);
        server_socket_fd_ = -1;
    }
    listening_ = false;
    logger_.Info("tcp_server_transport.cpp: Server disconnected.");
}

bool TcpServerTransport::is_connected() const
{
    return client_connected_;
}

TransportAsyncSendResult TcpServerTransport::async_send_data(std::vector<uint8_t>&& data)
{
    if (!client_connected_)
    {
        return TransportAsyncSendResult::NOT_CONNECTED;
    }
    if (!outgoing_queue_.try_enqueue(std::move(data)))
    {
        logger_.Warn("tcp_server_transport.cpp: Outgoing queue is full.");
        return TransportAsyncSendResult::QUEUE_FULL;
    }
    return TransportAsyncSendResult::SUCCESS;
}

void TcpServerTransport::sender_thread_func()
{
    while (sender_thread_running_)
    {
        std::vector<uint8_t> data;
        if (outgoing_queue_.try_dequeue(data))
        {
            if (client_connected_ && client_socket_fd_ >= 0)
            {
                ssize_t bytes_sent = ::send(client_socket_fd_, data.data(), data.size(), 0);
                if (bytes_sent < 0)
                {
                    logger_.Error("tcp_server_transport.cpp: Failed to send data: {}", strerror(errno));
                    handle_disconnect_detected();
                }
                else if (static_cast<size_t>(bytes_sent) != data.size())
                {
                    logger_.Warn("tcp_server_transport.cpp: Incomplete send. Sent {} of {} bytes.", bytes_sent, data.size());
                }
            }
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

void TcpServerTransport::handle_disconnect_detected()
{
    if (client_socket_fd_ >= 0)
    {
        ::close(client_socket_fd_);
        client_socket_fd_ = -1;
    }
    client_connected_ = false;
    logger_.Warn("tcp_server_transport.cpp: Client connection has been marked as disconnected.");
}

bool TcpServerTransport::accept_connection()
{
    if (!listening_)
    {
        logger_.Error("tcp_server_transport.cpp: Cannot accept, not listening.");
        return false;
    }
    if (client_connected_)
    {
        // Already have an active client connection. Do not log repeatedly or report a new accept.
        // Returning false prevents higher-level logic from treating this as a freshly accepted connection.
        return false;
    }

    logger_.Info("tcp_server_transport.cpp: Waiting to accept a new connection...");
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int new_socket = ::accept(server_socket_fd_, (struct sockaddr *)&client_addr, &client_len);

    if (new_socket < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            logger_.Error("tcp_server_transport.cpp: Failed to accept new connection: {}", strerror(errno));
        }
        return false;
    }

    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    logger_.Info("tcp_server_transport.cpp: Accepted connection from {}:{}", client_ip, ntohs(client_addr.sin_port));

    client_socket_fd_ = new_socket;
    client_connected_ = true;
    connection_timestamp_ = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();

    if (!sender_thread_running_)
    {
        sender_thread_running_ = true;
        sender_thread_ = std::thread(&TcpServerTransport::sender_thread_func, this);
    }

    return true;
}


bool TcpServerTransport::data_available(int timeout_ms)
{
    if (!client_connected_ || client_socket_fd_ < 0)
    {
        return false;
    }
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(client_socket_fd_, &readfds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    int result = ::select(client_socket_fd_ + 1, &readfds, NULL, NULL, timeout_ms >= 0 ? &tv : NULL);
    if (result < 0)
    {
        logger_.Error("tcp_server_transport.cpp: Select error in data_available: {}", strerror(errno));
        if (errno == EBADF)
        {
            handle_disconnect_detected();
        }
        return false;
    }
    return result > 0;
}

int TcpServerTransport::receive_data(void* buffer, size_t max_size)
{
    if (!client_connected_ || client_socket_fd_ < 0)
    {
        return -1;
    }
    ssize_t bytes_received = ::recv(client_socket_fd_, buffer, max_size, 0);
    if (bytes_received < 0)
    {
        if (errno != EWOULDBLOCK && errno != EAGAIN)
        {
            logger_.Error("tcp_server_transport.cpp: recv failed: {}", strerror(errno));
            handle_disconnect_detected();
        }
        return -1;
    }
    else if (bytes_received == 0)
    {
        logger_.Info("tcp_server_transport.cpp: Client disconnected gracefully.");
        handle_disconnect_detected();
        return 0;
    }
    return static_cast<int>(bytes_received);
}


bool TcpServerTransport::receive_exact(void* buffer, size_t size)
{
    if (!client_connected_ || client_socket_fd_ < 0)
    {
        return false;
    }
    size_t total_received = 0;
    while (total_received < size)
    {
        ssize_t bytes_received = ::recv(client_socket_fd_, static_cast<char*>(buffer) + total_received, size - total_received, 0);
        if (bytes_received < 0)
        {
            if (errno != EWOULDBLOCK && errno != EAGAIN)
            {
                logger_.Error("tcp_server_transport.cpp: recv failed in receive_exact: {}", strerror(errno));
                handle_disconnect_detected();
                return false;
            }
            // Spurious wakeup, continue trying
            continue;
        }
        else if (bytes_received == 0)
        {
            logger_.Warn("tcp_server_transport.cpp: Client disconnected while waiting for {} bytes. Got only {}.", size, total_received);
            handle_disconnect_detected();
            return false;
        }
        total_received += bytes_received;
    }
    return true;
}

} // namespace gateway