Of course. Here is a detailed design document for building the control plane. It synthesizes the information from our previous discussions and the provided documentation into a concrete implementation plan.

### My Understanding of Your Goal

**The Problem:** You have a robust, low-latency "data plane" (`RosGateway`) capable of transmitting ROS messages over TCP. However, this data plane is static and requires manual configuration. It lacks the intelligence to participate in a dynamic, managed offloading ecosystem. It cannot discover other services, negotiate connections, or reconfigure its data routing based on high-level system directives.

**Your Proposed Solution:** You want to build a "control plane" as a new layer of intelligence that owns and orchestrates the `RosGateway`. This document serves as a complete blueprint for that control plane. The goal is to create a system that can:
1.  **Discover Services:** Automatically connect to a `DiscoveryService` to learn the network topology and retrieve the address of a central "Bridge" component.
2.  **Negotiate and Manage Sessions:** Establish a persistent control connection to the Bridge, using a custom JSON-based protocol to request offloading, receive directives, and manage session lifecycles.
3.  **Expose a ROS 2 Interface:** Offer a ROS 2 service that allows other nodes in the VHC to request computational offloading.
4.  **Dynamically Configure the Data Plane:** Based on approval messages from the Bridge, dynamically create, register, and configure the necessary `MessageHandlerBase` instances within the `RosGateway` to route the correct ROS topics over the data plane.

This document provides sufficient detail for another AI assistant to implement the solution using your existing repository.

### Correction of Assumptions

Your high-level design and assumptions are sound. The dual-plane architecture is the correct model, and the requirements for reliability on the control channels are critical. The plan outlined below builds directly upon these correct assumptions without modification.

---

## Modular Gateway Control Plane: Implementation Specification

This document details the design and implementation steps for creating the Modular Gateway's control plane.

### 1. High-Level Architecture

The control plane will be implemented as a new primary class, `GatewayController`, which will be a ROS 2 node. This controller will orchestrate three main components:
1.  **The Data Plane (`RosGateway`):** The existing `RosGateway` class, which will be owned by the controller and used exclusively for high-speed data transfer.
2.  **The Discovery Client (`DiscoveryClient`):** A new helper class responsible for all communication with the `DiscoveryService`.
3.  **The Bridge Control Client (`BridgeControlClient`):** A new helper class responsible for all communication with the Bridge's control plane using the JSON protocol we designed.

The system will be multi-threaded to ensure responsiveness:
*   **ROS 2 Threads:** Managed by `rclcpp::spin`, handling incoming service calls.
*   **Control Logic Thread:** A dedicated thread within `GatewayController` running the main state machine.
*   **Network I/O Threads:** The existing sender/receiver threads within each `TransportLib` instance.

### 2. New Component: `GenericTopicHandler`

A critical prerequisite is the creation of a new, generic message handler. The existing handlers like `StringHandler` are too specific. The control plane must be able to handle arbitrary topics defined at runtime by the OM.

#### `generic_topic_handler.hpp`
This handler will be configurable at runtime with a topic name and message type. It will handle raw, serialized data.

````cpp
#ifndef GENERIC_TOPIC_HANDLER_HPP
#define GENERIC_TOPIC_HANDLER_HPP

#include "modular_gateway_sender/message_handler_base.hpp"
#include "rclcpp/generic_subscription.hpp"
#include "rclcpp/generic_publisher.hpp"
#include <map>

namespace gateway {

class GenericTopicHandler : public MessageHandlerBase {
public:
  // The handler is constructed with the specific topic and its ROS 2 message type string.
  GenericTopicHandler(
    RosGateway* gateway, 
    const std::string& handler_name,
    const std::string& topic_name, 
    const std::string& topic_type);
  
  void initialize() override;
  void shutdown() override;
  
  bool can_process_message_type(MessageType type) const override;
  
  bool process_and_publish_received_msg(
      const std::string& topic,
      MessageType type,
      const void* data,
      size_t size,
      const MessageOptions& options) override;
  
private:
  void generic_subscription_callback(const std::shared_ptr<rclcpp::SerializedMessage> msg);

  std::string topic_name_;
  std::string topic_type_; // e.g., "std_msgs/msg/String"
  MessageType message_type_enum_; // The gateway's internal enum for this topic

  rclcpp::GenericSubscription::SharedPtr subscription_;
  std::map<std::string, rclcpp::GenericPublisher::SharedPtr> publishers_;
};

} // namespace gateway

#endif // GENERIC_TOPIC_HANDLER_HPP
````

#### `generic_topic_handler.cpp`
The implementation will use `rclcpp::GenericSubscription` and `rclcpp::GenericPublisher` to handle serialized message data without needing to know the concrete type at compile time.

### 3. New Component: `DiscoveryClient`

This class encapsulates all interaction with the `DiscoveryService`.

#### `discovery_client.hpp`
````cpp
#ifndef DISCOVERY_CLIENT_HPP
#define DISCOVERY_CLIENT_HPP

#include "transport_base.hpp"
#include "discovery_protocol/protocol.hpp" // Assumes this is the path to the library
#include <thread>
#include <functional>
#include <atomic>

namespace gateway {

class DiscoveryClient {
public:
    using SuccessCallback = std::function<void(const discovery_protocol::RegistrationResponse&)>;
    using FailureCallback = std::function<void(const std::string&)>;

    DiscoveryClient(const std::string& host, int port, rclcpp::Logger logger);
    ~DiscoveryClient();

    void start(discovery_protocol::RegistrationRequest request, SuccessCallback sc, FailureCallback fc);
    void stop();

private:
    void client_thread_func();

    std::string host_;
    int port_;
    rclcpp::Logger logger_;
    
    std::unique_ptr<TransportBase> transport_;
    
    std::thread client_thread_;
    std::atomic<bool> running_{false};

    discovery_protocol::RegistrationRequest registration_request_;
    SuccessCallback success_cb_;
    FailureCallback failure_cb_;
};

} // namespace gateway
#endif // DISCOVERY_CLIENT_HPP
````

#### `discovery_client.cpp`
The implementation will manage a `TcpClientTransport`, connect to the service, send the registration request, and then enter a loop. The loop will use `transport_->data_available(timeout)` to both listen for responses and trigger sending `DISC:PNG` keepalives.

### 4. New Component: `BridgeControlClient`

This class is similar in structure to the `DiscoveryClient` but implements the JSON-based control protocol.

#### `bridge_control_client.hpp`
````cpp
#ifndef BRIDGE_CONTROL_CLIENT_HPP
#define BRIDGE_CONTROL_CLIENT_HPP

#include "transport_base.hpp"
#include "nlohmann/json.hpp" // Assuming nlohmann/json is used
#include <thread>
#include <functional>
#include <atomic>
#include <map>

namespace gateway {

// Forward declare message structs
struct SessionApproved;
struct SessionDenied;
struct SessionTeardownCommand;

class BridgeControlClient {
public:
    // Define callbacks for different events
    using SessionApprovedCallback = std::function<void(const SessionApproved&)>;
    // ... other callbacks for denied, teardown, etc.

    BridgeControlClient(const std::string& host, int port, rclcpp::Logger logger);
    ~BridgeControlClient();

    void start(/* required callbacks */);
    void stop();

    // Method to send an offload request
    void send_offload_request(const std::string& task_id, uint8_t vhc_group, uint8_t vhc_id);

private:
    void client_thread_func();
    void handle_received_message(const nlohmann::json& msg);
    void send_message_reliable(const nlohmann::json& msg);

    // Struct to hold messages pending an ACK
    struct PendingAck {
        nlohmann::json message;
        std::chrono::steady_clock::time_point sent_time;
        int retry_count;
    };

    std::string host_;
    int port_;
    rclcpp::Logger logger_;
    
    std::unique_ptr<TransportBase> transport_;
    
    std::thread client_thread_;
    std::atomic<bool> running_{false};
    std::atomic<uint32_t> sequence_number_{0};

    std::mutex pending_acks_mutex_;
    std::map<uint32_t, PendingAck> pending_acks_;

    // Callback members
    SessionApprovedCallback on_session_approved_;
};

} // namespace gateway
#endif // BRIDGE_CONTROL_CLIENT_HPP
````

### 5. New Component: `GatewayController`

This is the central orchestrator.

#### `gateway_controller.hpp`
````cpp
#ifndef GATEWAY_CONTROLLER_HPP
#define GATEWAY_CONTROLLER_HPP

#include "rclcpp/rclcpp.hpp"
#include "ros_gateway.hpp"
#include "discovery_client.hpp"
#include "bridge_control_client.hpp"
// #include "your_offload_service_interface.hpp" // The .srv definition

#include <thread>
#include <atomic>

namespace gateway {

class GatewayController : public rclcpp::Node {
public:
    GatewayController(const rclcpp::NodeOptions& options);
    ~GatewayController();

private:
    enum class State {
        INITIALIZING,
        DISCOVERING,
        CONNECTING_TO_BRIDGE,
        OPERATIONAL,
        FAILED
    };

    // Main state machine, runs in its own thread
    void control_thread_func();

    // Callbacks for DiscoveryClient
    void on_discovery_success(const discovery_protocol::RegistrationResponse& response);
    void on_discovery_failure(const std::string& reason);

    // Callbacks for BridgeControlClient
    void on_bridge_session_approved(/* args */);
    void on_bridge_teardown_directive(/* args */);

    // ROS 2 Service for offloading
    // void offload_request_service_cb(...);

    // State
    std::atomic<State> state_{State::INITIALIZING};

    // Data Plane
    std::unique_ptr<RosGateway> data_plane_;

    // Control Plane Clients
    std::unique_ptr<DiscoveryClient> discovery_client_;
    std::unique_ptr<BridgeControlClient> bridge_control_client_;

    // Main control thread
    std::thread control_thread_;
    std::atomic<bool> running_{false};

    // ROS 2 Interfaces
    // rclcpp::Service<...>::SharedPtr offload_service_;

    // Discovered bridge info
    std::string bridge_host_;
    int bridge_port_;
};

} // namespace gateway
#endif // GATEWAY_CONTROLLER_HPP
````

#### `gateway_controller.cpp`
The constructor will read ROS 2 parameters to get its identity (`groupId`, `idInGroup`, `componentName`) and the `DiscoveryService` address. It will then start the `control_thread_`.

The `control_thread_func` will implement the state machine:
1.  **`INITIALIZING`**: Create the `DiscoveryClient`.
2.  **`DISCOVERING`**: Call `discovery_client_->start()`, passing the registration request and the `on_discovery_success`/`on_discovery_failure` callbacks. The thread will then wait on a condition variable.
3.  **`CONNECTING_TO_BRIDGE`**: This state is entered when `on_discovery_success` is called. It will create the `BridgeControlClient` with the host/port from the discovery response and start it.
4.  **`OPERATIONAL`**: Entered when the `BridgeControlClient` successfully connects. The controller is now ready to process offloading requests forwarded from the ROS 2 service callback. When a directive arrives from the bridge, it will call `data_plane_->register_handler()` or `data_plane_->unregister_handler()` with instances of the new `GenericTopicHandler`.

### 6. New Main Executable

A new main file is required to run the controller.

#### `gateway_controller_main.cpp`
````cpp
#include "modular_gateway_sender/gateway_controller.hpp"
#include "rclcpp/rclcpp.hpp"

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  
  rclcpp::NodeOptions options;
  // Allow setting parameters from a launch file
  options.automatically_declare_parameters_from_overrides(true);

  auto controller_node = std::make_shared<gateway::GatewayController>(options);
  
  rclcpp::spin(controller_node);
  rclcpp::shutdown();
  return 0;
}
````

### 7. CMakeLists.txt Modifications

The following changes are needed in `CMakeLists.txt`.

1.  **Add new source files** to a new `control_plane_lib`.
2.  **Add the new `gateway_controller` executable.**

````cmake
// ... existing find_package and include_directories ...

# --- Library Definitions ---

# Add the new generic handler to the handlers library
add_library(handlers
  src/handlers/string_handler.cpp
  src/handlers/laserscan_handler.cpp
  src/handlers/string_test_input_handler.cpp  
  src/handlers/string_test_result_handler.cpp
  src/handlers/generic_topic_handler.cpp
)
# ... existing handlers target_link_libraries ...

# NEW: Control Plane Library
add_library(control_plane_lib
  src/discovery_client.cpp
  src/bridge_control_client.cpp
  src/gateway_controller.cpp
)
# Add include directories for nlohmann/json and discovery_protocol
# target_include_directories(control_plane_lib PUBLIC ...)
target_link_libraries(control_plane_lib
  PRIVATE
  ros_gateway_lib
  # discovery_protocol_lib # Link the discovery protocol library
  # nlohmann_json_lib      # Link the json library
)
ament_target_dependencies(control_plane_lib rclcpp)


# ... existing transport_lib, ros_gateway_lib, message_handler_base ...

# --- Executable Definitions ---
# ... existing COMMON_EXEC_LIBS ...

# NEW: Controller Executable
add_executable(gateway_controller src/gateway_controller_main.cpp)
target_link_libraries(gateway_controller control_plane_lib ${COMMON_EXEC_LIBS})
ament_target_dependencies(gateway_controller rclcpp)


# ... existing client/server/VHC/MEC executables ...

# --- Installation ---
install(TARGETS
  gateway_client
  gateway_server
  gateway_VHC
  gateway_MEC
  gateway_controller # Add new executable
  DESTINATION lib/${PROJECT_NAME}
)

install(TARGETS
  gateway_transport_lib
  ros_gateway_lib
  message_handler_base
  handlers
  control_plane_lib # Add new library
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# ... existing install(DIRECTORY) and ament_package() ...
````