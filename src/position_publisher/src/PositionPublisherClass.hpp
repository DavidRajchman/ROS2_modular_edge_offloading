#ifndef POSITION_PUBLISHER_CLASS
#define POSITION_PUBLISHER_CLASS


#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <geometry_msgs/msg/quaternion.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include "services/msg/position.hpp"
#include "../../../lib/math/ROS2_math.hpp"
#include "../../../lib/topicClasses/RobotPositionMessage.hpp"
#include "../../../lib/topicClasses/MapMessage.hpp"


using namespace std;

class PositionPublisher : public rclcpp::Node {
public:
    PositionPublisher();

private:
    //class attributes
    // float mapOriginX, mapOriginY, mapRes;
    bool mapData = false;
    
    
    //ROS transformation listener
    tf2_ros::Buffer tfBuffer;
    tf2_ros::TransformListener tfListener;

    //subscribers
    rclcpp::Subscription<tf2_msgs::msg::TFMessage>::SharedPtr tfSub;
    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr mapSub;

    std::shared_ptr<tf2_ros::TransformListener> tf_listener{nullptr};
    std::unique_ptr<tf2_ros::Buffer> tf_buffer;

    //methods
    void tfCb(const tf2_msgs::msg::TFMessage::SharedPtr msg);
    void mapCb(const nav_msgs::msg::OccupancyGrid::SharedPtr msg);

    //class for publisher
    RobotPositionMessage robotPositionMsg;
    MapMessage mapMessage;
};

#endif