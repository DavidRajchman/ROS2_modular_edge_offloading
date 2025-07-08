#ifndef DISCOVERY_CLIENT_HPP
#define DISCOVERY_CLIENT_HPP

#include "logging/logger.h"
#include "discovery_protocol/protocol.hpp"

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <functional>

namespace gateway {

// Forward declare to avoid including the full transport header
class TransportBase;

class DiscoveryClient {
public:
    // Callbacks to notify the GatewayController
    using DiscoverySuccessCallback = std::function<void(const std::string& bridge_host, int bridge_port)>;
    using DiscoveryFailureCallback = std::function<void(const std::string& error_message)>;

    DiscoveryClient();
    ~DiscoveryClient();

    /**
     * @brief Starts the discovery process.
     * 
     * Connects to the DiscoveryService and attempts to register the component.
     * This method is non-blocking and launches a dedicated thread for communication.
     * 
     * @param host The hostname or IP address of the DiscoveryService.
     * @param port The port of the DiscoveryService.
     * @param component_type The type of this component (e.g., "VHC" or "MEC").
     * @param component_name A human-readable name for this component.
     * @param success_cb Callback invoked on successful discovery.
     * @param failure_cb Callback invoked on any failure.
     * @return true if the client thread was started successfully, false otherwise.
     */
    bool start(
        const std::string& host,
        int port,
        discovery_protocol::ComponentType component_type,
        const std::string& component_name,
        DiscoverySuccessCallback success_cb,
        DiscoveryFailureCallback failure_cb
    );

    /**
     * @brief Stops the discovery client and its communication thread.
     */
    void stop();

private:
    void client_thread_func();

    std::unique_ptr<TransportBase> transport_;
    CppLogging::Logger logger_;

    std::thread client_thread_;
    std::atomic<bool> running_{false};

    // Connection and registration details
    std::string host_;
    int port_;
    discovery_protocol::ComponentType component_type_;
    std::string component_name_;

    // Callbacks
    DiscoverySuccessCallback success_cb_;
    DiscoveryFailureCallback failure_cb_;
};

} // namespace gateway

#endif // DISCOVERY_CLIENT_HPP