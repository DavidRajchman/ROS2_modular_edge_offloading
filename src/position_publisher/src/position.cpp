#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_msgs/msg/string.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <geometry_msgs/msg/point_stamped.hpp>
#include <geometry_msgs/msg/quaternion_stamped.hpp>
#include <sensor_msgs/msg/point_cloud.hpp>
#include <tf2_ros/buffer.h>
#include <tf2_ros/transform_listener.h>
#include <tf2_ros/transform_broadcaster.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.hpp>
#include "services/msg/position.hpp"
#include "services/msg/control_node.hpp"
#include "services/msg/status_node.hpp"

#include <iostream>
#include <vector>
#include <math.h>
#include <string>
#include <cstdio>

using namespace std;

class PositionPublisher : public rclcpp::Node {
public:
    bool turnOnNode = true;
    bool debug = false;
    int width, height, midMapX, midMapY;
    float mapOriginX, mapOriginY, mapRes, originX, originY;
    float poseRobX, poseRobY, poseRobPhi, poseLidX, poseLidY, poseLidPhi;
    int32_t mapRobX, mapRobY, mapLidX, mapLidY;
    bool mapData = false;
    bool transformReady = false;
    string status = "OK";

    // ROS 2 messages
    visualization_msgs::msg::Marker marker;
    geometry_msgs::msg::PointStamped pointBaselink, pointLidar, pointOut, pointOut2;
    geometry_msgs::msg::QuaternionStamped quatIn, quatOut;
    sensor_msgs::msg::PointCloud cloudIn, cloudOut;
    std_msgs::msg::String state;
    services::msg::Position pos;
    services::msg::StatusNode nodeStatus;
    nav_msgs::msg::OccupancyGrid grid2;

    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr markerPub;
    rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr mapPub;
    rclcpp::Publisher<services::msg::Position>::SharedPtr posPub;
    rclcpp::Publisher<services::msg::StatusNode>::SharedPtr statusPub;

    rclcpp::Subscription<tf2_msgs::msg::TFMessage>::SharedPtr subTf;
    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr mapSub;
    rclcpp::Subscription<services::msg::ControlNode>::SharedPtr controlSub;

    tf2_ros::Buffer tfBuffer;
    std::shared_ptr<tf2_ros::TransformListener> tfListener;

    PositionPublisher() : Node("position_publisher"), tfBuffer(this->get_clock()), tfListener(std::make_shared<tf2_ros::TransformListener>(tfBuffer)) {
        // initParameters();

        // Publishers and Subscribers
        posPub = this->create_publisher<services::msg::Position>("/robot_position", 1);
        statusPub = this->create_publisher<services::msg::StatusNode>("/status", 1);
        controlSub = this->create_subscription<services::msg::ControlNode>("control_topic", 1, std::bind(&PositionPublisher::controlCb, this, std::placeholders::_1));
        mapSub = this->create_subscription<nav_msgs::msg::OccupancyGrid>("map", 1, std::bind(&PositionPublisher::mapCb, this, std::placeholders::_1));
        subTf = this->create_subscription<tf2_msgs::msg::TFMessage>("tf", 1, std::bind(&PositionPublisher::tfCb, this, std::placeholders::_1));

        if (debug) {
            markerPub = this->create_publisher<visualization_msgs::msg::Marker>("/poziceRobot", 1);
            mapPub = this->create_publisher<nav_msgs::msg::OccupancyGrid>("/map3", 1);
            setupMarker();
        }

        // Initialize Transform
        initializeTransform();
    }

    void controlCb(const services::msg::ControlNode::SharedPtr msg) {
        turnOnNode = msg->position_publisher;
    }

    void tfCb(const tf2_msgs::msg::TFMessage::SharedPtr msg) {
        if (!turnOnNode || msg->transforms[0].header.frame_id != "map" && msg->transforms[0].header.frame_id != "world") {
            return;
        }

        try {
            auto point_out = tfBuffer.transform(pointBaselink, "map", tf2::TimePointZero);
            auto point_out2 = tfBuffer.transform(pointLidar, "map", tf2::TimePointZero);
            auto quat_out = tfBuffer.transform(quatIn, "map", tf2::TimePointZero);

            // Position and orientation
            poseRobX = point_out.point.x;
            poseRobY = point_out.point.y;
            poseRobPhi = tf2::getYaw(quat_out.quaternion);

            poseLidX = point_out2.point.x;
            poseLidY = point_out2.point.y;
            poseLidPhi = poseRobPhi;

            if (mapData) {
                calculateGridPosition();
                posPub->publish(pos);

                if (debug) {
                    marker.pose.position.x = poseRobX;
                    marker.pose.position.y = poseRobY;
                    markerPub->publish(marker);
                }
            }
        } catch (tf2::TransformException &ex) {
            RCLCPP_WARN(this->get_logger(), "Transform failure: %s", ex.what());
        }
    }

    void mapCb(const nav_msgs::msg::OccupancyGrid::SharedPtr msg) {
        if (!turnOnNode) return;

        width = msg->info.width;
        height = msg->info.height;
        originX = -msg->info.origin.position.x;
        originY = -msg->info.origin.position.y;
        mapRes = msg->info.resolution;

        mapOriginX = round(originX / mapRes);
        mapOriginY = round(originY / mapRes);
        mapData = true;

        if (debug && transformReady) {
            grid2 = *msg;
            int index = mapRobX + width * mapRobY;
            grid2.data[index] = 100;
            mapPub->publish(grid2);
        }
    }

    void publishStatus() {
        nodeStatus.node = "position";
        nodeStatus.status = status;
        statusPub->publish(nodeStatus);
    }

private:
    void initializeTransform() {
        for (int i = 0; i < 25; ++i) {
            try {
                auto transform = tfBuffer.lookupTransform("map", "laser", tf2::TimePointZero);
                RCLCPP_INFO(this->get_logger(), "Transformation OK");
                transformReady = true;
                break;
            } catch (tf2::TransformException &ex) {
                rclcpp::sleep_for(std::chrono::milliseconds(200));
            }
        }
        if (!transformReady) {
            status = "ERROR: no transformation";
        }
    }

    void setupMarker() {
        marker.header.frame_id = "map";
        marker.type = visualization_msgs::msg::Marker::CUBE;
        marker.scale.x = 0.1;
        marker.scale.y = 0.1;
        marker.scale.z = 0.1;
        marker.color.r = 1.0;
        marker.color.g = 1.0;
        marker.color.b = 0.0;
        marker.color.a = 1.0;
        marker.pose.orientation.w = 1.0;
    }

    void calculateGridPosition() {
        mapRobX = round(poseRobX / mapRes) + mapOriginX;
        mapRobY = round(poseRobY / mapRes) + mapOriginY;
        mapLidX = round(poseLidX / mapRes) + mapOriginX;
        mapLidY = round(poseLidY / mapRes) + mapOriginY;
        pos.pose_rob_x = poseRobX;
        pos.pose_rob_y = poseRobY;
        pos.pose_rob_phi = poseRobPhi;
    }
};

int main(int argc, char **argv) {
    rclcpp::init(argc, argv);
    auto node = std::make_shared<PositionPublisher>();
    node->publishStatus();
    RCLCPP_INFO(node->get_logger(), "Position publisher OK");

    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}

