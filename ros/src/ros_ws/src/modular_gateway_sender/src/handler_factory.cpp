#include "modular_gateway_sender/handler_factory.hpp"
#include "modular_gateway_sender/ros_gateway.hpp"

// Include all concrete handler headers here so the factory knows about them.
#include "modular_gateway_sender/handlers/string_handler.hpp"
#include "modular_gateway_sender/handlers/laserscan_handler.hpp"
#include "modular_gateway_sender/handlers/string_test_input_handler.hpp"
#include "modular_gateway_sender/handlers/string_test_result_handler.hpp"

namespace gateway {

HandlerFactory::HandlerFactory(RosGateway* gateway, std::shared_ptr<rclcpp::Node> node)
    : gateway_(gateway), node_(node), logger_(CppLogging::Logger("gateway"))
{
    logger_.Info("handler_factory.cpp: HandlerFactory constructed.");
    register_known_handlers();
}

void HandlerFactory::register_known_handlers() {
    // Map string identifiers to lambda functions that create the handler instance.
    // These strings should match what the Bridge/OM sends in the session approval.
    
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

    // New: MessageType-based creators (canonical registry)
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

std::shared_ptr<MessageHandlerBase> HandlerFactory::create_handler(const std::string& handler_type_name) {
    auto it = creator_map_.find(handler_type_name);
    if (it == creator_map_.end()) {
        logger_.Error("handler_factory.cpp: No handler creator registered for type '{}'", handler_type_name);
        return nullptr;
    }

    logger_.Info("handler_factory.cpp: Creating handler for type '{}'", handler_type_name);
    // Execute the stored lambda function to create the handler
    return it->second(gateway_, node_);
}

std::shared_ptr<MessageHandlerBase> HandlerFactory::get_or_create(MessageType type) {
    // Reuse existing instance if present
    auto found = instances_by_type_.find(type);
    if (found != instances_by_type_.end()) {
        return found->second;
    }
    auto it = mt_creator_map_.find(type);
    if (it == mt_creator_map_.end()) {
        logger_.Error("handler_factory.cpp: No MessageType creator registered for id {}", static_cast<int>(type));
        return nullptr;
    }
    auto inst = it->second(gateway_, node_);
    if (inst) {
        instances_by_type_[type] = inst;
    }
    return inst;
}

} // namespace gateway