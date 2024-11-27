#include "PositionPublisherClass.hpp"

PositionPublisher::PositionPublisher() : Node("position_publisher"), tfBuffer(this->get_clock()), tfListener(tfBuffer)
{
    this->get_logger().set_level(rclcpp::Logger::Level::Debug);

    // transformation listeners init
    tf_buffer = std::make_unique<tf2_ros::Buffer>(this->get_clock());
    tf_listener = std::make_shared<tf2_ros::TransformListener>(*tf_buffer);
    tfLidar = std::make_shared<geometry_msgs::msg::TransformStamped>();
    tf = std::make_shared<tf2_msgs::msg::TFMessage>();

    //setting "UDP" connection
    auto qos_profile = rclcpp::QoS(0).reliability(rclcpp::ReliabilityPolicy::BestEffort);


    // Publishers and Subscribers
    posPub = this->create_publisher<services::msg::Position>("/robot_position", 1);
    tfPub = this->create_publisher<tf2_msgs::msg::TFMessage>("/tf", 1);
    mapSub = this->create_subscription<nav_msgs::msg::OccupancyGrid>("/map", 1, std::bind(&PositionPublisher::mapCb, this, std::placeholders::_1));
    tfSub = this->create_subscription<tf2_msgs::msg::TFMessage>("/tf", qos_profile, std::bind(&PositionPublisher::tfCb, this, std::placeholders::_1));
    if (debug)
    {
        markerPub = this->create_publisher<visualization_msgs::msg::Marker>("/position_marker", 1);
        mapPub = this->create_publisher<nav_msgs::msg::OccupancyGrid>("/map3", 1);
        setupMarker();
    }

    RCLCPP_INFO(this->get_logger(), "Position publisher OK");
}



void PositionPublisher::tfCb(const tf2_msgs::msg::TFMessage::SharedPtr msg)
{
    RCLCPP_DEBUG_ONCE(get_logger(), "tf received");

    if (!(msg->transforms[0].header.frame_id == "map" || msg->transforms[0].header.frame_id == "odom"))
        return;

    try
    {
        time = msg->transforms[0].header.stamp;
        geometry_msgs::msg::TransformStamped tfRob = tfBuffer.lookupTransform("map", "base_footprint", tf2::TimePointZero);
        RCLCPP_INFO_ONCE(this->get_logger(), "Transformation received");
        poseRobX = tfRob.transform.translation.x;
        poseRobY = tfRob.transform.translation.y;
        // poseRobPhi=quaternionToYaw(tfRob.transform.rotation);
        poseRobPhi=quaternionToYaw(
            (double)tfRob.transform.rotation.x,
            (double)tfRob.transform.rotation.y,
            (double)tfRob.transform.rotation.z,
            (double)tfRob.transform.rotation.w
        );

        calculateGridPosition();
        createPositionMessage();

        posPub->publish(pos);
        RCLCPP_INFO_ONCE(get_logger(), "Robot position published");
    }
    catch (tf2::TransformException &ex)
    {
        RCLCPP_WARN_ONCE(this->get_logger(), "Transform failure: %s", ex.what());
    }
}



void PositionPublisher::mapCb(const nav_msgs::msg::OccupancyGrid::SharedPtr msg)
{
    mapWidth = msg->info.width;
    mapHeight = msg->info.height;
    originX = -msg->info.origin.position.x;
    originY = -msg->info.origin.position.y;
    mapRes = msg->info.resolution;

    mapOriginX = round(originX / mapRes);
    mapOriginY = round(originY / mapRes);
    mapData = true;

    if (debug)
    {
        grid2 = *msg;
        int index = mapRobX + mapWidth * mapRobY;
        grid2.data[index] = 100;
        mapPub->publish(grid2);
    }
}

void PositionPublisher::setupMarker()
{
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

void PositionPublisher::createPositionMessage()
{
    // robot position
    pos.pose_rob_x = poseRobX;
    pos.pose_rob_y = poseRobY;
    pos.pose_rob_phi = poseRobPhi;

    // robot position in grid
    pos.map_rob_x = mapRobY;
    pos.map_rob_y = mapRobX;

    // map origin
    pos.map_origin_x = mapOriginX;
    pos.map_origin_y = mapOriginY;
}

void PositionPublisher::calculateGridPosition()
{
    mapRobY = round(poseRobX / mapRes) + mapOriginX;
    mapRobX = round(poseRobY / mapRes) + mapOriginY;
    mapLidX = round(poseLidX / mapRes) + mapOriginX;
    mapLidY = round(poseLidY / mapRes) + mapOriginY;
}


