#include "MotorMessage.hpp"

MotorMessage::MotorMessage(rclcpp::Logger logger) :logger(logger){}
MotorMessage::MotorMessage(rclcpp::Logger logger, rclcpp::Publisher<services::msg::ControlMotor>::SharedPtr publisher): logger(logger), publisher(publisher), clock(RCL_SYSTEM_TIME){}


void MotorMessage::setSpeed(char speed){
    speed = clamp99_99(speed);
    this -> speed = speed;
}

char MotorMessage::getSpeed(){
    return speed;
}


void MotorMessage::setSteer(char steer){
    steer = clamp99_99(steer);
    this -> steer = steer;
}

char MotorMessage::getSteer(){
    return steer;
}

void MotorMessage::setMode(string mode){
    if (mode !="auto" && mode != "manual"){
        RCLCPP_WARN(logger, "invalid mode");
        return;
    }
    this -> mode = mode;
    
}
string MotorMessage::getMode(){
    return mode;
}

void MotorMessage::publishMsg(){
    services::msg::ControlMotor msg;

    msg.mode= "";
    msg.forwarding = speed;
    msg.steering = steer;

    publisher -> publish(msg);
}

void MotorMessage::publishMode(string mode){
    setMode(mode);

    services::msg::ControlMotor msg;
    
    msg.mode= mode;
    msg.forwarding = 0.0;
    msg.steering = 0.0;

    publisher -> publish(msg);
}

void MotorMessage::publishMsg(services::msg::ControlMotor::SharedPtr message){
    receiveMsg(message);
    publishMsg();
}

void MotorMessage::receiveMsg(services::msg::ControlMotor::SharedPtr message){
    if (mode != ""){
        mode = message -> mode;
        return;
    }

    speed = message -> forwarding;
    steer = message -> steering;
}

