#ifndef GOAL_MESSAGE
#define GOAl_MESSAGE

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <cmath>
#include "../math/ROS2_math.hpp"

using namespace std;


class GoalMessage {
public:
    GoalMessage(rclcpp::Logger logger);
    GoalMessage(rclcpp::Logger logger, rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr publisher);

    void setPosition(double x, double y, double phi);
    void setPosition(int mapX, int mapY, int indexPhi, int mapOriginX, int mapOriginY, double resolution);
    tuple<double, double, double> getPosition();


    void receiveMsg(geometry_msgs::msg::PoseStamped::SharedPtr message);
    void publishMsg(geometry_msgs::msg::PoseStamped::SharedPtr message);
    void publishMsg();

private:
    rclcpp::Logger logger;
    rclcpp::Publisher<geometry_msgs::msg::PoseStamped>::SharedPtr publisher = nullptr;
    rclcpp::Clock clock;

    double x,y,phi;

};

#endif
