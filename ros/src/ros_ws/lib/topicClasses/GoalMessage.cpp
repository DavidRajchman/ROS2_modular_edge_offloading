#include "GoalMessage.hpp"


GoalMessage::GoalMessage(rclcpp::Logger logger) : logger(logger){}
GoalMessage::GoalMessage(rclcpp::Logger logger, rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr publisher): logger(logger), publisher(publisher), clock(RCL_SYSTEM_TIME){}

void GoalMessage::setPosition(double x, double y, double phi){
    this -> x = x; 
    this -> y = y; 
    this -> phi = phi; 
}

void GoalMessage::setPosition(int x, int y, int phi, int mapOriginX, int mapOriginY, double resolution){
    auto [poseX, poseY, posePhi]= gridPositionToPosition(x,y,phi, mapOriginX, mapOriginY, resolution);
    setPosition(poseX, poseY, posePhi);
}

tuple<double, double, double> GoalMessage::getPosition(){
    return {x,y,phi};
}


void GoalMessage::receiveMsg(geometry_msgs::msg::PoseStamped::SharedPtr message){
    x = message -> pose.position.x;
    y = message -> pose.position.y;
    phi = quaternionToYaw(
        message -> pose.orientation.x,
        message -> pose.orientation.y,
        message -> pose.orientation.z,
        message -> pose.orientation.w
    );
}

void GoalMessage::publishMsg(geometry_msgs::msg::PoseStamped::SharedPtr message){
    receiveMsg(message);
    publishMsg();
}

void GoalMessage::publishMsg(){
    geometry_msgs::msg::PoseStamped msg;

    msg.header.stamp = clock.now();
    msg.header.frame_id = "map";

    msg.pose.position.x = x;
    msg.pose.position.y = y;
    msg.pose.position.z = 0.0;

    auto [orientationX, orientationY, orientationZ, orientationW] = yawToQuaternion(phi);
    msg.pose.orientation.x = orientationX;
    msg.pose.orientation.y = orientationY;
    msg.pose.orientation.z = orientationZ;
    msg.pose.orientation.w = orientationW;

    publisher -> publish(msg);
}

