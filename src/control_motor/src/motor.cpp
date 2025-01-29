#include <rclcpp/rclcpp.hpp>
// #include <std_msgs/msg/int32.hpp>
// #include <std_msgs/msg/string.hpp>
#include <services/msg/control_motor.hpp>
// #include <services/msg/status_node.hpp>
#include "../../../lib/topicClasses/MotorUart.hpp"

// #include <fcntl.h>
// #include <errno.h>
// #include <termios.h>
// #include <unistd.h>
// #include <cstring>
// #include <iostream>
// #include <string>
// #include <thread>
// #include <array>


using namespace std;


class ControlMotor : public rclcpp::Node {
public:
    // MotorUart myClass = nullptr;
    char forwarding;
    char steering;
    char lastSteer = 0;
    int serial_port;
    // Status status = Status::OK;
    int curSpeed = 0;
    char command[7];
    rclcpp::Subscription<services::msg::ControlMotor>::SharedPtr subControl;
    // rclcpp::Publisher<services::msg::StatusNode>::SharedPtr statusPub;
    // services::msg::StatusNode nodeStatus;

    ControlMotor() : Node("control_motor"), motor(this ->get_logger() ) {
        //class for communication with teensy (motor) 
        // motor = MotorUart(this -> get_logger());

        // subscribers and publishers
        subControl = this->create_subscription<services::msg::ControlMotor>("controlMotor", 5,std::bind(&ControlMotor::motorCb, this, std::placeholders::_1));
        // statusPub = this->create_publisher<services::msg::StatusNode>("/status", 1);

        sleep(1);
    }

private:
    MotorUart motor;

    void motorCb(const services::msg::ControlMotor::SharedPtr msg) {
        // RCLCPP_INFO(this -> get_logger(), msg->mode);
        if(msg->mode == ""){
            RCLCPP_INFO(this -> get_logger(), "Control msg");
            motor.publishControl(static_cast<char>(msg->forwarding),static_cast<char>(msg->steering));
        }
        else if(msg->mode == "manual")
            motor.publishMode(0);
        else if(msg->mode == "auto")
            motor.publishMode(1);
        else if(msg->mode == "lidar")
            motor.setLidarSpeed(static_cast<char>(msg->forwarding));
        else
            RCLCPP_WARN(this->get_logger(), "Undefined command");
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<ControlMotor>();
    // node->publishStatus();
    RCLCPP_INFO(node->get_logger(), "Control motor OK");

    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    executor.spin();

    rclcpp::shutdown();
    return 0;
}