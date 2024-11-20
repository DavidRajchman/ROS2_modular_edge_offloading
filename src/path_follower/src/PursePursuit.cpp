#include "PursePursuitClass.hpp"

using namespace std;


int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = make_shared<PurePursuit>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}