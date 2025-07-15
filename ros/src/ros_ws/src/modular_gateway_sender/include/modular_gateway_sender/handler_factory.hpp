#ifndef HANDLER_FACTORY_HPP
#define HANDLER_FACTORY_HPP

#include "logging/logger.h"
#include "rclcpp/rclcpp.hpp"

#include <string>
#include <functional>
#include <map>
#include <memory>

namespace gateway {

// Forward declarations to reduce header includes
class MessageHandlerBase;
class RosGateway;

class HandlerFactory {
public:
    // A CreatorFunc is a function that can create a specific handler instance.
    using CreatorFunc = std::function<std::shared_ptr<MessageHandlerBase>(RosGateway*, std::shared_ptr<rclcpp::Node>)>;

    HandlerFactory(RosGateway* gateway, std::shared_ptr<rclcpp::Node> node);

    /**
     * @brief Creates a new message handler instance based on its type name.
     * 
     * @param handler_type_name The string identifier for the handler type (e.g., "std_msgs/msg/String").
     * @return A shared_ptr to the new handler, or nullptr if the type is unknown.
     */
    std::shared_ptr<MessageHandlerBase> create_handler(const std::string& handler_type_name);

private:
    void register_known_handlers();

    RosGateway* gateway_;
    std::shared_ptr<rclcpp::Node> node_;
    std::map<std::string, CreatorFunc> creator_map_;
    CppLogging::Logger logger_;
};

} // namespace gateway

#endif // HANDLER_FACTORY_HPP