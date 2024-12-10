#include "PositionPublisherClass.hpp"

PositionPublisher::PositionPublisher() : Node("position_publisher"), tfBuffer(this->get_clock()), tfListener(tfBuffer),
           robotPositionMsg(this->get_logger(), this->create_publisher<services::msg::Position>("/robot_position", 1)),
            mapMessage(this->get_logger())
{
    this->get_logger().set_level(rclcpp::Logger::Level::Debug);

    // transformation listeners init
    tf_buffer = std::make_unique<tf2_ros::Buffer>(this->get_clock());
    tf_listener = std::make_shared<tf2_ros::TransformListener>(*tf_buffer);

    //setting "UDP" connection
    auto qos_profile = rclcpp::QoS(0).reliability(rclcpp::ReliabilityPolicy::BestEffort);


    //Subscribers
    mapSub = this->create_subscription<nav_msgs::msg::OccupancyGrid>("/map", 1, std::bind(&PositionPublisher::mapCb, this, std::placeholders::_1));
    tfSub = this->create_subscription<tf2_msgs::msg::TFMessage>("/tf", qos_profile, std::bind(&PositionPublisher::tfCb, this, std::placeholders::_1));

    RCLCPP_INFO(this->get_logger(), "Position publisher OK");
}



void PositionPublisher::tfCb(const tf2_msgs::msg::TFMessage::SharedPtr msg)
{
    //receive tf message and transform using tf listeners to map frame
    //afterward, publish simpler data for other nodes
    RCLCPP_DEBUG_ONCE(get_logger(), "tf received");

    if (!(msg->transforms[0].header.frame_id == "map" || msg->transforms[0].header.frame_id == "odom"))
        return;
    try
    {
        geometry_msgs::msg::TransformStamped tfRob = tfBuffer.lookupTransform("map", "base_footprint", tf2::TimePointZero);
        RCLCPP_INFO_ONCE(this->get_logger(), "Transformation received");
        double poseRobPhi=quaternionToYaw(
            (double)tfRob.transform.rotation.x,
            (double)tfRob.transform.rotation.y,
            (double)tfRob.transform.rotation.z,
            (double)tfRob.transform.rotation.w
        );
        robotPositionMsg.setPosition(
            tfRob.transform.translation.x,
            tfRob.transform.translation.y,
            poseRobPhi
        );

        robotPositionMsg.publishMsg();

        RCLCPP_INFO_ONCE(get_logger(), "Robot position published");
    }
    catch (tf2::TransformException &ex)
    {
        RCLCPP_WARN_ONCE(this->get_logger(), "Transform failure: %s", ex.what());
    }
}


void PositionPublisher::mapCb(const nav_msgs::msg::OccupancyGrid::SharedPtr msg)
{
    // receive map message fortaking info about map dimension and origin for robot_position topic
    mapMessage.receiveMsg(msg);
    auto [sizeX, sizeY, resolution] = mapMessage.getMapDimensions();
    auto [mapOriginX, mapOriginY, mapOriginPhi] = mapMessage.getMapOriginGrid();

    RCLCPP_DEBUG_STREAM(this->get_logger(), "sizeX: "<<sizeX<<" sizeY: "<<sizeY<<" resolution: "<<resolution);
    RCLCPP_DEBUG_STREAM(this->get_logger(), "orinX: "<<mapOriginX<<" orinY: "<<mapOriginY<<" orinPhi: "<<mapOriginPhi);

    robotPositionMsg.setMapDimensions(sizeX, sizeY, resolution);
    robotPositionMsg.setMapOrigin(mapOriginX, mapOriginY);
}


