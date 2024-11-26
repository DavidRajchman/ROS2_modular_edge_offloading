#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>
#include <services/msg/control_motor.hpp>
#include <services/msg/status_node.hpp>

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <array>

#include "controlMotorMessage.hpp"

using namespace std;

enum class Status {
    OK,
    NO_CONNECTION,
    ERROR
};

class ControlMotor : public rclcpp::Node {
public:
    // MotorUart myClass = nullptr;
    char forwarding;
    char steering;
    char lastSteer = 0;
    int serial_port;
    Status status = Status::OK;
    int curSpeed = 0;
    char command[7];
    rclcpp::Subscription<services::msg::ControlMotor>::SharedPtr subControl;
    rclcpp::Publisher<services::msg::StatusNode>::SharedPtr statusPub;
    services::msg::StatusNode nodeStatus;

    ControlMotor() : Node("control_motor") {
        // auto log = this->get_logger();
        // myClass = make_shared<MotorUart>();
        // logger = get_logger();
        // open connection with microcontroller
        struct termios tty;
        serial_port = open("/dev/arduino", O_RDWR); //TODO jetson
        if (tcgetattr(serial_port, &tty) != 0) {
            string err = strerror(errno);
            status = Status::ERROR;
            RCLCPP_ERROR(this->get_logger(), "Error %i from tcgetattr: %s", errno, err.c_str());
        } else {
            RCLCPP_WARN(this->get_logger(), "Communication with Arduino OK");
        }

        tty.c_cflag &= ~PARENB;
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CSIZE;
        tty.c_cflag |= CS8;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cflag |= CREAD | CLOCAL;
        tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
        tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
        tty.c_oflag &= ~(OPOST | ONLCR);
        tty.c_cc[VTIME] = 10;
        tty.c_cc[VMIN] = 0;
        cfsetispeed(&tty, B115200);
        cfsetospeed(&tty, B115200);

        if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
            string err = strerror(errno);
            status = Status::ERROR;
            RCLCPP_ERROR(this->get_logger(), "Error %i from tcsetattr: %s", errno, err.c_str());
        }

        // subscribers and publishers
        subControl = this->create_subscription<services::msg::ControlMotor>("controlMotor", 5,std::bind(&ControlMotor::motorCb, this, std::placeholders::_1));
        statusPub = this->create_publisher<services::msg::StatusNode>("/status", 1);

        sleep(3);
    }

    void motorCb(const services::msg::ControlMotor::SharedPtr msg) {
        if(msg -> mode == "manual" ||  msg -> mode == "auto"){
            changeAutonomousMode(msg -> mode);
        } 
        else if(msg -> mode == ""){
            controlSpeedAndSteer(msg->forwarding, msg->steering);
        } 
        else {
            RCLCPP_WARN(this->get_logger(), "Undefined command");
        }
    }
    void createCommand(int speed, int steer){
        string strSpeed= to_string(speed);
        string strSteer= to_string(steer);
        // char command[7] ;

        command[0] = speed >=0 ? '0' : '1';
        command[1] = strSpeed[strSpeed.size()-1];
        command[2] = strSpeed[strSpeed.size()-2];
        command[3] = steer >=0 ? '0' : '1';
        command[4] = strSteer[strSteer.size()-1];
        command[5] = strSteer[strSteer.size()-2];
        command[6] = '\n';

    }

    void createCommand(string mode){
        command[0] = '9';
        command[1] = mode=="auto" ? '9' : '0';
        command[2] = mode=="auto" ? '9' : '0';
        command[3] = '9';
        command[4] = mode=="auto" ? '9' : '0';
        command[5] = mode=="auto" ? '9' : '0';
        command[6] = '\n';

    }

    void controlSpeedAndSteer(int speed, int steer) {
        speed = clamp99(speed);
        steer = clamp99(steer);
        createCommand(speed, steer);
        // speed = speed * (-1);
        // curSpeed = speed;
        // string stringSpeed = to_string(speed);
        // char command[7] = {'v', ' ', ' ', ' ', ' ', ' ', '\n'};
        // for (size_t i = 0; i < stringSpeed.size(); i++) {
            // command[i + 2] = stringSpeed[i];
        write(serial_port, command, sizeof(command));
        usleep(10000);
    }

    int clamp99(int number){
        if(number >99)
            return 99;
        else if (number <-99)
            return -99;
        else
            return number;
    }


    void changeAutonomousMode(string mode) {
        createCommand(mode);
        write(serial_port, command, sizeof(command));
        usleep(50000);
    }


    // void changeSpeed(int speed) {
    //     speed = speed * (-1);
    //     curSpeed = speed;
    //     std::string stringSpeed = std::to_string(speed);
    //     char command[7] = {'v', ' ', ' ', ' ', ' ', ' ', '\n'};
    //     for (size_t i = 0; i < stringSpeed.size(); i++) {
    //         command[i + 2] = stringSpeed[i];
    //     }
    //     write(serial_port, command, sizeof(command));
    //     usleep(50000);
    // }

    // void changeSteer(int steer) {
    //     steer = steer * (-1);
    //     if (lastSteer == steer) return;
    //     lastSteer = steer;
    //     std::string stringSteer = std::to_string(steer);
    //     char command[7] = {'s', ' ', ' ', ' ', ' ', ' ', '\n'};
    //     for (size_t i = 0; i < stringSteer.size(); i++) {
    //         command[i + 2] = stringSteer[i];
    //     }
    //     write(serial_port, command, sizeof(command));
    //     usleep(50000);
    // }

    // void changeAutonomousMode(bool power) {
    //     const signed char msg[] = {power ? 'm' : 'n', '\n'};
    //     write(serial_port, msg, sizeof(msg));
    //     usleep(100000);
    // }

    void publishStatus() {
        nodeStatus.node = "motor";
        switch (status){
            case Status::OK:
                nodeStatus.status = "OK";
                break;
            case Status::ERROR:
                nodeStatus.status = "ERROR";
                break;
            case Status::NO_CONNECTION:
                nodeStatus.status = "NO_CONNECTION";
                break;
        }
        // nodeStatus.status = status;
        statusPub->publish(nodeStatus);
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<ControlMotor>();
    node->publishStatus();
    RCLCPP_INFO(node->get_logger(), "Control motor OK");

    rclcpp::executors::MultiThreadedExecutor executor;
    executor.add_node(node);
    executor.spin();

    rclcpp::shutdown();
    return 0;
}