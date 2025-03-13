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
#include <fstream>
#include <string>
#include <thread>
#include <array>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>

using namespace std;


class MotorUart {
public:
    MotorUart(rclcpp::Logger logger);
    ~MotorUart();


    void setSpeed(float speed);
    float getSpeed();

    void setSteer(float rotation);
    float getSteer();

    void setMode(char mode);
    char getMode();

    void setLidarSpeed(char speed);
    char getLidarSpeed();

    void publishControl();
    void publishControl(float speed, float steer);

    void publishLidarSpeed(); 
    void publishLidarSpeed(char speed);

    void publishMode(); 
    void publishMode(char mode); 

    
    private:
    rclcpp::Logger logger ;
    void setConnection(string dev = "/dev/arduino");
    void readData();


    float speed;
    float steer;
    float rotation;
    char mode;
    unsigned char lidarSpeed;
    int serial_port;
    fstream serialPort;
    // TODO lenghtx of AV
    const double BASE_WHEEL = 0.5;
};

#endif
