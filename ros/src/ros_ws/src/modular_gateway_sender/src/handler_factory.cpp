#include "modular_gateway_sender/handler_factory.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

// Include all concrete handler headers here so the factory knows about them.
#include "modular_gateway_sender/handlers/string_handler.hpp"
#include "modular_gateway_sender/handlers/laserscan_handler.hpp"
#include "modular_gateway_sender/handlers/string_test_input_handler.hpp"
#include "modular_gateway_sender/handlers/string_test_result_handler.hpp"

namespace gateway {

// Constructor: Initialize factory with ROS gateway and node references
// Automatically registers all known handler types via register_known_handlers()
HandlerFactory::HandlerFactory(RosGateway* gateway, std::shared_ptr<rclcpp::Node> node)
    : gateway_(gateway), node_(node), logger_(CppLogging::Logger("gateway"))
{
    logger_.Info("handler_factory.cpp: HandlerFactory constructed.");
    register_known_handlers();
}

// Register all known handler types with factory
// Maintains two registries:
// 1. Legacy string-based (creator_map_) for backward compatibility with SESSION_APPROVED
// 2. MessageType enum-based (mt_creator_map_) for canonical type identification
void HandlerFactory::register_known_handlers() {
    // Legacy string-based registry - matches ROS2 message type names
    // Used when Bridge sends handler_type as string in SESSION_APPROVED
    
    creator_map_["std_msgs/msg/String"] = [](RosGateway* gw, std::shared_ptr<rclcpp::Node> n) {
        return std::make_shared<StringHandler>(gw, n);
    };

    creator_map_["sensor_msgs/msg/LaserScan"] = [](RosGateway* gw, std::shared_ptr<rclcpp::Node> n) {
        return std::make_shared<LaserScanHandler>(gw, n);
    };

    creator_map_["test/string_input"] = [](RosGateway* gw, std::shared_ptr<rclcpp::Node> n) {
        return std::make_shared<StringTestInputHandler>(gw, n);
    };

    creator_map_["test/string_result"] = [](RosGateway* gw, std::shared_ptr<rclcpp::Node> n) {
        return std::make_shared<StringTestResultHandler>(gw, n);
    };

    // MessageType enum-based registry (canonical approach)
    // Maps MessageType enum values (1=STRING, 11=LASERSCAN, etc.) to handler creators
    mt_creator_map_[MessageType::STRING] = [](RosGateway* gw, std::shared_ptr<rclcpp::Node> n) {
        return std::make_shared<StringHandler>(gw, n);
    };
    mt_creator_map_[MessageType::smLASERSCAN] = [](RosGateway* gw, std::shared_ptr<rclcpp::Node> n) {
        return std::make_shared<LaserScanHandler>(gw, n);
    };
    mt_creator_map_[MessageType::STRING_TEST_INPUT] = [](RosGateway* gw, std::shared_ptr<rclcpp::Node> n) {
        return std::make_shared<StringTestInputHandler>(gw, n);
    };
    mt_creator_map_[MessageType::STRING_TEST_RESULT] = [](RosGateway* gw, std::shared_ptr<rclcpp::Node> n) {
        return std::make_shared<StringTestResultHandler>(gw, n);
    };

    logger_.Info("handler_factory.cpp: Registered {} legacy string types and {} MessageType creators.", creator_map_.size(), mt_creator_map_.size());
}

// Create handler by string type name (legacy approach)
// Used when processing SESSION_APPROVED with string-based handler_type
// Returns nullptr if type not registered
std::shared_ptr<MessageHandlerBase> HandlerFactory::create_handler(const std::string& handler_type_name) {
    auto it = creator_map_.find(handler_type_name);
    if (it == creator_map_.end()) {
        logger_.Error("handler_factory.cpp: No handler creator registered for type '{}'", handler_type_name);
        return nullptr;
    }

    logger_.Info("handler_factory.cpp: Creating handler for type '{}'", handler_type_name);
    // Execute the stored lambda function to create the handler instance
    return it->second(gateway_, node_);
}

// Get or create handler by MessageType enum (singleton pattern per type)
// Reuses existing handler instance if already created for this MessageType
// Caches instances in instances_by_type_ map to avoid duplicate handlers
// Returns nullptr if MessageType not registered in mt_creator_map_
std::shared_ptr<MessageHandlerBase> HandlerFactory::get_or_create(MessageType type) {
    // Check if handler instance already exists for this type
    auto found = instances_by_type_.find(type);
    if (found != instances_by_type_.end()) {
        return found->second;
    }
    
    // Create new handler instance using registered creator lambda
    auto it = mt_creator_map_.find(type);
    if (it == mt_creator_map_.end()) {
        logger_.Error("handler_factory.cpp: No MessageType creator registered for id {}", static_cast<int>(type));
        return nullptr;
    }
    auto inst = it->second(gateway_, node_);
    if (inst) {
        instances_by_type_[type] = inst;  // Cache for future reuse
    }
    return inst;
}

} // namespace gateway