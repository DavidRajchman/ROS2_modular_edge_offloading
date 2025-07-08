#include "modular_gateway_sender/gateway_controller.hpp"
#include "modular_gateway_sender/transport/tcp_server_transport.hpp"
#include "modular_gateway_sender/discovery_client.hpp"
#include "discovery_protocol/protocol.hpp"

// These will be created in a subsequent step
// #include "modular_gateway_sender/handler_factory.hpp"

namespace gateway {

GatewayController::GatewayController(const rclcpp::NodeOptions& options)
  : rclcpp::Node("gateway_controller", options),
    logger_(CppLogging::Logger("gateway"))
{
  logger_.Info("gateway_controller.cpp: Constructing GatewayController...");

  // Declare and load all required parameters
  this->declare_parameter<std::string>("discovery_service.host", "127.0.0.1");
  this->declare_parameter<int>("discovery_service.port", 19900);
  this->declare_parameter<std::string>("identity.component_type", "VHC");
  this->declare_parameter<std::string>("identity.component_name", "default_vhc");
  this->declare_parameter<int>("identity.group_id", 1);
  this->declare_parameter<int>("identity.id_in_group", 1);
  this->declare_parameter<int>("data_plane.listen_port", 7401);

  discovery_host_ = this->get_parameter("discovery_service.host").as_string();
  discovery_port_ = this->get_parameter("discovery_service.port").as_int();
  component_type_ = this->get_parameter("identity.component_type").as_string();
  component_name_ = this->get_parameter("identity.component_name").as_string();
  id_group_ = this->get_parameter("identity.group_id").as_int();
  identifier_in_group_ = this->get_parameter("identity.id_in_group").as_int();
  data_plane_listen_port_ = this->get_parameter("data_plane.listen_port").as_int();
  component_id_ = std::to_string(id_group_) + ":" + std::to_string(identifier_in_group_);

  // Create the data plane transport and the RosGateway instance
  auto data_plane_transport = std::make_unique<TcpServerTransport>(data_plane_listen_port_, 1);
  gateway_ = std::make_unique<RosGateway>(this->shared_from_this(), std::move(data_plane_transport));
  gateway_->set_identity(static_cast<uint8_t>(id_group_), static_cast<uint8_t>(identifier_in_group_));

  // Create the handler factory
  handler_factory_ = std::make_unique<HandlerFactory>(gateway_.get(), this->shared_from_this());

  // Create the ROS 2 service for offloading requests
  offloading_service_ = this->create_service<modular_gateway_sender::srv::RequestOffloading>(
    "request_offloading",
    std::bind(&GatewayController::offloading_request_service_handler, this, std::placeholders::_1, std::placeholders::_2)
  );

  // Start the main control logic thread
  running_ = true;
  control_thread_ = std::thread(&GatewayController::control_thread_func, this);
  
  logger_.Info("gateway_controller.cpp: GatewayController constructed successfully. Control thread started.");
}

GatewayController::~GatewayController()
{
  logger_.Info("gateway_controller.cpp: Shutting down GatewayController...");
  running_ = false;
  
  // Notify all condition variables to unblock threads
  state_cv_.notify_all();
  queue_cv_.notify_all();

  if (discovery_client_) {
    discovery_client_->stop();
  }
  if (bridge_cp_client_) {
    bridge_cp_client_->stop();
  }

  if (control_thread_.joinable()) {
    control_thread_.join();
  }
  if (data_plane_connection_thread_.joinable()) {
    data_plane_connection_thread_.join();
  }
  if (gateway_) {
    gateway_->stop_receiver();
  }
  
  logger_.Info("gateway_controller.cpp: GatewayController shut down.");
}

void GatewayController::offloading_request_service_handler(
  const std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Request> request,
  std::shared_ptr<modular_gateway_sender::srv::RequestOffloading::Response> response)
{
    if (state_.load() != State::OPERATIONAL) {
        logger_.Warn("gateway_controller.cpp: Offloading request for task '{}' received, but controller is not operational. Rejecting.", request->task_id);
        response->approved = false;
        response->message = "Gateway is not in OPERATIONAL state.";
        return;
    }

    logger_.Info("gateway_controller.cpp: Queuing offloading request for task_id: {}", request->task_id);
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        offloading_request_queue_.push({request->task_id});
    }
    queue_cv_.notify_one();

    // For now, we reply immediately. In a real system, we might wait for a future/promise.
    response->approved = true;
    response->message = "Request queued for processing by the bridge.";
}

void GatewayController::control_thread_func() {
    while(running_) {
        State current_state = state_.load();
        logger_.Info("gateway_controller.cpp: Control thread processing state: {}", static_cast<int>(current_state));

        switch(current_state) {
            case State::INITIALIZING:
            {
                // Start the data plane server listening for connections
                auto transport = dynamic_cast<TcpServerTransport*>(gateway_->get_transport());
                if (transport && transport->connect()) {
                    data_plane_connection_thread_ = std::thread(&GatewayController::data_plane_connection_thread_func, this);
                } else {
                    logger_.Error("gateway_controller.cpp: CRITICAL: Failed to start Data Plane server. Entering FAILED state.");
                    state_ = State::FAILED;
                    break;
                }
                
                // Transition to the discovery state
                state_ = State::DISCOVERING;
                break;
            }
            case State::DISCOVERING:
            {
                logger_.Info("gateway_controller.cpp: Starting discovery process...");
                discovery_client_ = std::make_unique<DiscoveryClient>();
                
                auto component_type_enum = discovery_protocol::string_to_component_type(component_type_);

                discovery_client_->start(
                    discovery_host_,
                    discovery_port_,
                    component_type_enum,
                    component_name_,
                    std::bind(&GatewayController::on_discovery_success, this, std::placeholders::_1, std::placeholders::_2),
                    std::bind(&GatewayController::on_discovery_failure, this, std::placeholders::_1)
                );

                // Wait for discovery to complete (or fail)
                std::unique_lock<std::mutex> lock(state_mutex_);
                state_cv_.wait(lock, [this]{ return state_ != State::DISCOVERING || !running_; });
                break;
            }
            case State::CONNECTING_TO_BRIDGE:
            {
                bridge_cp_client_ = std::make_unique<BridgeCpClient>();
                
                // Define callbacks for the bridge client
                auto dp_confirmed_cb = std::bind(&GatewayController::on_dp_confirmed, this);
                auto session_approved_cb = std::bind(&GatewayController::on_session_approved, this, std::placeholders::_1);
                auto session_denied_cb = std::bind(&GatewayController::on_session_denied, this, std::placeholders::_1, std::placeholders::_2);

                if (!bridge_cp_client_->start(bridge_cp_host_, bridge_cp_port_, dp_confirmed_cb, session_approved_cb, session_denied_cb)) {
                    logger_.Error("gateway_controller.cpp: CRITICAL: Could not connect to Bridge Control Plane. Retrying in 5s...");
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    // Stay in this state to retry on the next loop iteration
                    break; 
                }
                
                logger_.Info("gateway_controller.cpp: Bridge Control Plane client connected. Sending DP Info...");
                std::string my_ip = "127.0.0.1"; // TODO: Get this dynamically
                bridge_cp_client_->send_dp_info(component_id_, my_ip, data_plane_listen_port_);
                
                logger_.Info("gateway_controller.cpp: DP Info sent. Waiting for DP confirmation from Bridge.");
                state_ = State::WAITING_FOR_DP_CONNECTION;
                break;
            }
            case State::WAITING_FOR_DP_CONNECTION:
            {
                // The on_dp_confirmed() callback will change the state.
                // We wait here until that happens.
                logger_.Info("gateway_controller.cpp: Waiting for Data Plane connection to be confirmed by Bridge...");
                std::unique_lock<std::mutex> lock(state_mutex_);
                state_cv_.wait(lock, [this]{ return state_ != State::WAITING_FOR_DP_CONNECTION || !running_; });
                break;
            }
            case State::OPERATIONAL:
            {
                // In operational state, we process the offloading request queue.
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this]{ return !offloading_request_queue_.empty() || !running_; });

                if (!running_) break;

                OffloadingRequestData request = offloading_request_queue_.front();
                offloading_request_queue_.pop();
                lock.unlock();

                logger_.Info("gateway_controller.cpp: Dequeued and processing offloading request for task '{}'", request.task_id);
                
                // TODO: Look up task_name from a task database using task_id
                std::string task_name = "task_name_placeholder"; 
                std::string request_id = component_id_ + ":" + std::to_string(request_id_counter_++);

                bridge_cp_client_->send_offload_request(component_id_, request_id, request.task_id, task_name);
                break;
            }
            case State::FAILED:
            {
                logger_.Error("gateway_controller.cpp: Controller has entered FAILED state. Halting operations.");
                running_ = false; // Stop the loop
                break;
            }
        }
    }
    logger_.Info("gateway_controller.cpp: Control thread exited.");
}

void GatewayController::data_plane_connection_thread_func()
{
  TransportBase* transport = gateway_->get_transport();
  if (!transport) {
      logger_.Error("gateway_controller.cpp: Transport is null in data plane thread. Shutting down.");
      state_ = State::FAILED;
      state_cv_.notify_one();
      return;
  }

  while (running_) {
    if (!transport->is_connected()) {
      if (gateway_->is_receiver_running()) {
        logger_.Warn("gateway_controller.cpp: Data plane disconnected. Stopping receiver.");
        gateway_->stop_receiver();
      }
      logger_.Info("gateway_controller.cpp: Data plane server listening. Waiting for Bridge DP to connect...");
      
      if (transport->accept_connection()) {
        logger_.Info("gateway_controller.cpp: Data plane connection ACCEPTED from Bridge DP.");
        // The connection is accepted, but we wait for the Bridge to *confirm* it via the control plane.
        // The on_dp_confirmed() callback is the trigger to start the receiver.
      } else {
          if (running_) {
            logger_.Warn("gateway_controller.cpp: Failed to accept data plane connection. Retrying in 2s...");
            std::this_thread::sleep_for(std::chrono::seconds(2));
          }
      }
    } else {
        // If we are connected, just sleep. The receiver is managed by state transitions.
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  }
}

// --- Callback Implementations ---

void GatewayController::on_discovery_success(const std::string& bridge_host, int bridge_port) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ == State::DISCOVERING) {
        logger_.Info("gateway_controller.cpp: Discovery successful. Bridge CP at {}:{}", bridge_host, bridge_port);
        bridge_cp_host_ = bridge_host;
        bridge_cp_port_ = bridge_port;
        state_ = State::CONNECTING_TO_BRIDGE;
    }
    state_cv_.notify_one();
}

void GatewayController::on_discovery_failure(const std::string& error_message) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    logger_.Error("gateway_controller.cpp: Discovery failed: {}. Retrying in 10s.", error_message);
    // Instead of failing, we will retry discovery after a delay
    std::this_thread::sleep_for(std::chrono::seconds(10));
    if (state_ == State::DISCOVERING) {
        // The loop in control_thread_func will re-initiate discovery
    }
    state_cv_.notify_one(); // Wake up the control thread to retry
}

void GatewayController::on_dp_confirmed() {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (state_ == State::WAITING_FOR_DP_CONNECTION) {
        logger_.Info("gateway_controller.cpp: DP connection confirmed by Bridge. Starting receiver and transitioning to OPERATIONAL.");
        gateway_->start_receiver();
        state_ = State::OPERATIONAL;
    } else {
        logger_.Warn("gateway_controller.cpp: Received DP confirmation in unexpected state: {}", static_cast<int>(state_.load()));
    }
    state_cv_.notify_one();
}

void GatewayController::on_session_approved(const std::string& request_id) {
    logger_.Info("gateway_controller.cpp: Session with request_id '{}' approved by Bridge.", request_id);
    
    // TODO: Look up task details from a session map using the request_id.
    // The task details should include a list of message types to handle.
    // For now, we'll use a hardcoded example list.
    std::vector<std::string> required_handlers = {"std_msgs/msg/String", "sensor_msgs/msg/LaserScan"};

    for (const auto& handler_type : required_handlers) {
        auto handler = handler_factory_->create_handler(handler_type);
        if (handler) {
            gateway_->register_handler(handler);
            // TODO: Configure handler mode (Subscriber/Publisher) based on task details.
            // For now, default to subscriber only.
            MessageHandlerBase::configure_handler_mode(handler, HandlerMode::SUBSCRIBER_ONLY);
        }
    }
}

void GatewayController::on_session_denied(const std::string& request_id, const std::string& reason) {
    logger_.Warn("gateway_controller.cpp: Session with request_id '{}' denied by Bridge: {}", request_id, reason);
    // TODO: Clean up any state related to this request_id
}

} //