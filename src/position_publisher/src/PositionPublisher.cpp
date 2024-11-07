#include <rclcpp/rclcpp.hpp>
#include "PositionPublisherClass.hpp"



int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<PositionPublisher>();
    // node->publishStatus();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}