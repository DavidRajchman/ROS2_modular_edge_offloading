#ifndef MOTOR_UART
#define MOTOR_UART

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>
#include <services/msg/control_motor.hpp>
#include "../math/ROS2_math.hpp"


using namespace std;


class MotorMessage {
public:
    MotorMessage(rclcpp::Logger logger);
    MotorMessage(rclcpp::Logger logger, rclcpp::Publisher<services::msg::ControlMotor>::SharedPtr publisher);


    void setSpeed(char speed);
    char getSpeed();

    void setSteer(char steer);
    char getSteer();

    void setMode(string mode);
    string getMode();

    void publishMsg();
    void publishMsg(char speed, char steer);
    void publishMode(string mode);
    void publishMsg(services::msg::ControlMotor::SharedPtr message);
    void receiveMsg(services::msg::ControlMotor::SharedPtr message);

private:
    rclcpp::Logger logger ;
    rclcpp::Publisher<services::msg::ControlMotor>::SharedPtr publisher;
    rclcpp::Clock clock;



    char speed;
    char steer;
    string mode;

};

#endif
