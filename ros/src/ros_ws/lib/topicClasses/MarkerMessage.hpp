#ifndef MARKER_MESSAGE
#define MARKER_MESSAGE

#include <rclcpp/rclcpp.hpp>
#include <visualization_msgs/msg/marker.hpp>
#include <cmath>
#include "../math/ROS2_math.hpp"

using namespace std;


class MarkerMessage {
public:
    MarkerMessage(rclcpp::Logger logger);
    MarkerMessage(rclcpp::Logger logger, rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr publisher);

    void setColor(double red, double green, double blue, double alpha);
    void setScale(double x, double y, double z);
    void setPosition(tuple<int, int, int> poseGrid, int mapOriginX, int mapOriginY, double mapResolution);
    void setPosition(tuple<double, double, double> pose);
    void setType(string action);
    void setID(int id);
    int getID();

    void receiveMsg(visualization_msgs::msg::Marker::SharedPtr message);
    void publishMsg(visualization_msgs::msg::Marker::SharedPtr message);
    void publishMsg();

private:
    rclcpp::Logger logger;
    rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr publisher = nullptr;
    rclcpp::Clock clock;

    double red, green, blue, alpha;
    double x, y, phi;
    double scaleX, scaleY, scaleZ;
    int id = 0;
    int action = 0;
};

#endif
