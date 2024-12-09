#ifndef PURE_PURSUIT_CLASS
#define PURE_PURSUIT_CLASS

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <nav_msgs/msg/path.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <services/msg/control_motor.hpp>
#include <services/msg/position.hpp>
#include "../../../../lib/topicClasses/PathMessage.hpp"
#include "../../../../lib/topicClasses/RobotPositionMessage.hpp"
#include "../../../../lib/topicClasses/MotorMessage.hpp"


#include <vector>
#include <string>

using namespace std;

struct MotorMsg{
    string mode;
    int speed;
    int steer;
};

class PurePursuit : public rclcpp::Node {
public:
    PurePursuit();

private:
    //parameters
    bool control;
    double poseRobX, poseRobY, poseRobPhi;
    int indexFollow;
    vector<pair<float, float>> path;
    vector<tuple<double, double, double>> pathFollow;

    bool DEBUG;
    double MAX_DIST, LOOK_DISTANCE;
    double KP_LOW, KP_HIGH;
    double sumAngle = 0;


    // subscribers and publishers
    rclcpp::Subscription<services::msg::Position>::SharedPtr poseSub;
    rclcpp::Subscription<nav_msgs::msg::Path>::SharedPtr pathSub;
    rclcpp::Publisher<services::msg::ControlMotor>::SharedPtr motorPub;

    //callbacks
    void poseCb(const services::msg::Position::SharedPtr msg);
    void pathCb(const nav_msgs::msg::Path::SharedPtr msg);

    //methods
    // void publishMode(string mode);
    // void controlMotor(int speed, int steer);
    void followPath();
    void findAngle(float Kp, double pursuitX, double pursuitY);

    //classes for messages
    RobotPositionMessage positionMsg;
    PathMessage pathMsg;
    MotorMessage motorMsg; 

};

#endif