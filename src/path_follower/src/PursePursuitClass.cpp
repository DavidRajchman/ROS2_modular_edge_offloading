#include "PursePursuitClass.hpp"

using namespace std;

PurePursuit::PurePursuit() : Node("pure_pusuit"), control(false),
    positionMsg(this->get_logger()),
    pathMsg(this->get_logger()),
    motorMsg(this->get_logger(), this->create_publisher<services::msg::ControlMotor>("control_motor", 1))
{
    RCLCPP_INFO(this->get_logger(), "Initializing PurePursuit node...");

    // parameters
    DEBUG = this->declare_parameter<bool>("DEBUG", false);
    MAX_DIST = this->declare_parameter<double>("MAX_DISTANCE_DELETE_POINT", 0.5);
    LOOK_DISTANCE = this->declare_parameter<double>("LOOK_DISTANCE", 1.0);
    KP_LOW = this->declare_parameter<double>("KP_LOW", 1.5);
    KP_HIGH = this->declare_parameter<double>("KP_HIGH", 5.0);

    // subscribers and publishers
    poseSub = this->create_subscription<services::msg::Position>("robot_position", 1, std::bind(&PurePursuit::poseCb, this, std::placeholders::_1));
    pathSub = this->create_subscription<nav_msgs::msg::Path>("path", 1, std::bind(&PurePursuit::pathCb, this, std::placeholders::_1));
}

void PurePursuit::poseCb(const services::msg::Position::SharedPtr msg)
{
    positionMsg.receiveMsg(msg);
    if (control)
    {
        followPath();
    }
}

void PurePursuit::pathCb(const nav_msgs::msg::Path::SharedPtr msg)
{
    pathMsg.receiveMsg(msg);
    pathFollow = pathMsg.getPath(); 

    followPath();

    control=true;
    motorMsg.publishMode("auto");
}


void PurePursuit::followPath()
{
    int i = 0;
    float distance = 0;
    int pathSize = static_cast<int>(pathFollow.size());

    while (distance < MAX_DIST && !path.empty())
    {
        distance = sqrt(pow(poseRobX - path[0].first, 2) + pow(poseRobY - path[0].second, 2));

        // remove point from queue
        if (distance < MAX_DIST)
        {
            path.erase(path.begin());
        }

        double pursuitX, pursuitY;
        // find point in certain distance
        if (indexFollow != pathSize - 2)
        {
            int j = indexFollow;
            float distance1 = abs(sqrt(pow(poseRobX - get<0>(pathFollow[j]), 2) + pow(poseRobY - get<1>(pathFollow[j]), 2)));
            float distance2 = abs(sqrt(pow(poseRobX - get<0>(pathFollow[j + 1]), 2) + pow(poseRobY - get<1>(pathFollow[j + 1]), 2)));

            while ((!(distance2 > LOOK_DISTANCE) && j + 1 < pathSize) || (distance1 > distance2 && pathSize > j + 1))
            {
                j++;
                distance1 = abs(sqrt(pow(poseRobX - get<0>(pathFollow[j]), 2) + pow(poseRobY - get<1>(pathFollow[j]), 2)));
                distance2 = abs(sqrt(pow(poseRobX - get<0>(pathFollow[j + 1]), 2) + pow(poseRobY - get<1>(pathFollow[j + 1]), 2)));
            }

            float ratio = (1 - distance1 / LOOK_DISTANCE) / (distance2 / LOOK_DISTANCE - distance1 / LOOK_DISTANCE);
            ratio = clamp(ratio, 0.0f, 1.0f);

            // find point on the line between points
            pursuitX = (get<0>(pathFollow[j + 1]) - get<0>(pathFollow[j])) * ratio + get<0>(pathFollow[j]);
            pursuitY = (get<1>(pathFollow[j + 1]) - get<1>(pathFollow[j])) * ratio + get<1>(pathFollow[j]);
            indexFollow = j;

            i++;
        }
        else if (indexFollow == pathSize - 2)
        { // final point
            pursuitX = get<0>(pathFollow.back());
            pursuitY = get<1>(pathFollow.back());

            i++;
        }

        // stop condition
        if (path.empty())
        {
            RCLCPP_INFO(this->get_logger(), "Following finished.");

            control = false;
            motorMsg.publishMode("manual");
            return;
        }

        findAngle(KP_HIGH, pursuitX, pursuitY);
    }
}

void PurePursuit::findAngle(float Kp, double pursuitX, double pursuitY) {
    float difX, difY, angleMap;
    int angle;

    difX = pursuitX - poseRobX;
    difY = pursuitY - poseRobY;

    // distance between current position and next point
    angleMap = std::atan2(difY, difX);
    double difAngle = poseRobPhi - angleMap;
    difAngle = clampPI_PI(difAngle);

    // PI regulator
    sumAngle -= difAngle;
    if (control) {
        angle = std::round(Kp * (-difAngle * 180 / M_PI) + sumAngle * 0.2); // 0.35

        // Anti-windup
        if (sumAngle > 60) {
            sumAngle = 60;
        } else if (sumAngle < -60) {
            sumAngle = -60;
        }

        //publish speed and steer
        motorMsg.publishMsg(18, clamp99_99(angle));
    } else {
        sumAngle = 0;
    }
}
