#ifndef MOTOR_UART
#define MOTOR_UART

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>
#include <services/msg/control_motor.hpp>
#include "../math/ROS2_math.hpp"

#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <array>

using namespace std;


class MotorUart {
public:
    MotorUart(rclcpp::Logger logger);

    void setSpeed(char speed);
    char getSpeed();

    void setSteer(char steer);
    char getSteer();

    void setMode(char mode);
    char getMode();

    void setLidarSpeed(char speed);
    char getLidarSpeed();

    void publishControl();
    void publishControl(char speed, char steer);

    void publishLidarSpeed(); 
    void publishLidarSpeed(char speed);

    void publishMode(); 
    void publishMode(char mode); 

private:
    rclcpp::Logger logger ;
    void setConnection(string dev = "/dev/arduino");


    char speed;
    char steer;
    char mode;
    char lidarSpeed;
    int serial_port;

};

#endif
