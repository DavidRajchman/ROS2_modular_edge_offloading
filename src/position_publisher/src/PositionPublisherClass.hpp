#ifndef POSITION_PUBLISHER_CLASS
#define POSITION_PUBLISHER_CLASS


#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <geometry_msgs/msg/quaternion.hpp>
#include <rosgraph_msgs/msg/clock.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include "services/msg/position.hpp"
#include "services/msg/control_node.hpp"
#include "../../../lib/math/ROS2_math.hpp"

#include "../../../lib/topicClasses/RobotPositionMessage.hpp"

#include <iostream>
#include <string>
#include <cstdio>

using namespace std;

class PositionPublisher : public rclcpp::Node {
public:
    PositionPublisher();

private:
    //class attributes
    bool debug= this->declare_parameter<bool>("DEBUG", false);
    int mapWidth, mapHeight, midMapX, midMapY;
    float mapOriginX, mapOriginY, originX, originY;
    float mapRes;
    float poseRobX, poseRobY, poseRobPhi;
    float poseLidX, poseLidY, poseLidPhi;
    int32_t mapRobX, mapRobY, mapLidX, mapLidY;
    bool mapData = false;

    //ROS messages
    visualization_msgs::msg::Marker marker;
    geometry_msgs::msg::PointStamped pointBaselink, pointLidar, pointOut, pointOut2;
    tf2_msgs::msg::TFMessage::SharedPtr tf;
    geometry_msgs::msg::TransformStamped::SharedPtr tfLidar;
    services::msg::Position pos;
    nav_msgs::msg::OccupancyGrid grid2;
    builtin_interfaces::msg::Time time;
    
    
    //ROS transformation listener
    tf2_ros::Buffer tfBuffer;
    tf2_ros::TransformListener tfListener;
    rclcpp::TimerBase::SharedPtr timerLidar;

    //publishers
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr markerPub;
    rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr mapPub;
    rclcpp::Publisher<services::msg::Position>::SharedPtr posPub;
    rclcpp::Publisher<tf2_msgs::msg::TFMessage>::SharedPtr tfPub;
    
    //subscribers
    rclcpp::Subscription<tf2_msgs::msg::TFMessage>::SharedPtr tfSub;
    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr mapSub;
    rclcpp::Subscription<services::msg::ControlNode>::SharedPtr controlSub;
    rclcpp::Subscription<rosgraph_msgs::msg::Clock>::SharedPtr timeSub;

    std::shared_ptr<tf2_ros::TransformListener> tf_listener{nullptr};
    std::unique_ptr<tf2_ros::Buffer> tf_buffer;


    //methods
    void lidarTfCb();
    void tfCb(const tf2_msgs::msg::TFMessage::SharedPtr msg);
    void mapCb(const nav_msgs::msg::OccupancyGrid::SharedPtr msg);
    void setupMarker();
    void calculateGridPosition();
    void createLidarTransformation();
    void createPositionMessage();

    rclcpp::TimerBase::SharedPtr timer_;

};

#endif