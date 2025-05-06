#include <transport/logging_utils.hpp>
#include <transport/transport_base.hpp> // For TcpClientTransport, TcpServerTransport

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring> // For strcmp, strlen

const int TEST_PORT = 12345;
const char* TEST_HOST = "127.0.0.1";

void run_server() {
    gateway::TcpServerTransport server(TEST_PORT);
    LOG_INFO("Server: Starting...");

    if (!server.connect()) { // Starts listening
        LOG_ERROR("Server: Failed to start listening.");
        return;
    }
    // The explicit client acceptance loop has been removed.
    // Based on the comment "// accept_connection() is non-blocking and called within receive_exact/send_data",
    // we infer that receive_exact() will handle waiting for a client connection.
    LOG_INFO("Server: Listening on port %d. Waiting for client connection and initial data...", TEST_PORT);

    char buffer[256];
    const std::string expected_message = "Hello from client!";
    LOG_INFO("Server: Attempting to receive %zu bytes (this will also wait for a client).", expected_message.length() + 1);

    if (server.receive_exact(buffer, expected_message.length() + 1)) {
        buffer[expected_message.length()] = '\0'; // Ensure null termination
        LOG_INFO("Server: Received: '%s'", buffer);

        if (std::string(buffer) == expected_message) {
            LOG_INFO("Server: Message matches expected.");
            const std::string reply_message = "Hello from server!";
            LOG_INFO("Server: Sending reply: '%s'", reply_message.c_str());
            if (!server.send_data(reply_message.c_str(), reply_message.length() + 1)) {
                LOG_ERROR("Server: Failed to send reply.");
            } else {
                LOG_INFO("Server: Reply sent successfully.");
            }
        } else {
            LOG_ERROR("Server: Message mismatch. Expected '%s', got '%s'.", expected_message.c_str(), buffer);
        }
    } else {
        // Updated error message to reflect that failure could be due to various reasons,
        // including no client connecting, client disconnecting, or a receive error,
        // as receive_exact() is now presumed to handle the connection attempt.
        LOG_ERROR("Server: Failed to receive data. This could be due to no client connecting, client disconnecting, or a receive error.");
    }

    server.disconnect();
    LOG_INFO("Server: Shut down.");
}

void run_client() {
    // Give the server a moment to start listening
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    gateway::TcpClientTransport client(TEST_HOST, TEST_PORT);
    LOG_INFO("Client: Attempting to connect to %s:%d...", TEST_HOST, TEST_PORT);

    if (!client.connect()) {
        LOG_ERROR("Client: Failed to connect to server.");
        return;
    }
    LOG_INFO("Client: Connected to server.");

    const std::string message = "Hello from client!";
    LOG_INFO("Client: Sending: '%s'", message.c_str());

    if (client.send_data(message.c_str(), message.length() + 1)) {
        LOG_INFO("Client: Message sent successfully.");

        char recv_buffer[256];
        const std::string expected_reply = "Hello from server!";
        LOG_INFO("Client: Attempting to receive reply (%zu bytes)...", expected_reply.length() + 1);

        if (client.receive_exact(recv_buffer, expected_reply.length() + 1)) {
            recv_buffer[expected_reply.length()] = '\0'; // Ensure null termination
            LOG_INFO("Client: Received reply: '%s'", recv_buffer);
            if (std::string(recv_buffer) == expected_reply) {
                LOG_INFO("Client: Reply matches expected. Test PASSED.");
            } else {
                LOG_ERROR("Client: Reply mismatch. Expected '%s', got '%s'. Test FAILED.", expected_reply.c_str(), recv_buffer);
            }
        } else {
            LOG_ERROR("Client: Failed to receive reply or server disconnected.");
        }
    } else {
        LOG_ERROR("Client: Failed to send message.");
    }

    client.disconnect();
    LOG_INFO("Client: Disconnected.");
}

int main(int argc, char* argv[]) {
    LOG_INFO("Bridge application - TransportLib Test");
    LOG_INFO("--------------------------------------");

    std::thread server_thread(run_server);
    std::thread client_thread(run_client);

    if (client_thread.joinable()) {
        client_thread.join();
    }
    if (server_thread.joinable()) {
        server_thread.join();
    }

    LOG_INFO("--------------------------------------");
    LOG_INFO("TransportLib test finished.");
    
    // You can add your original bridge logic here or let it exit.
    // For this test, we'll just exit.
    // std::cout << "Bridge is running. Press Ctrl+C to exit." << std::endl;
    // while (true) {
    //     std::this_thread::sleep_for(std::chrono::seconds(1));
    // }
    
    return 0;
}