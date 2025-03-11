#ifndef PATH_MESSAGE
#define PATH_MESSAGE

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/path.hpp>
#include <cmath>
#include "../math/ROS2_math.hpp"

using namespace std;


class PathMessage {
public:
    PathMessage(rclcpp::Logger logger);
    PathMessage(rclcpp::Logger logger, rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr publisher);


    //publishing and receiving messages
    void publishMsg();
    void publishMsg(const nav_msgs::msg::Path::SharedPtr message);
    void receiveMsg(const nav_msgs::msg::Path::SharedPtr message);

    vector<tuple<double, double, double>>getPath();
    vector<tuple<int, int, int>>getPathGrid();
    void setPath(vector<tuple<double, double, double>> path);
    void setPath(vector<tuple<int, int, int>> pathGrid, int mapOriginX, int mapOriginY, double resolution);
    void setPath(vector<pair<int, int>> pathGrid, int mapOriginX, int mapOriginY, double resolution);
    static void setMapDimensions(int mapOriginX, int mapOriginY, double mapResolution);
    static tuple<int, int, double> getMapDimensions();

private:
    rclcpp::Logger logger;
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr publisher = nullptr;
    nav_msgs::msg::Path msg;
    rclcpp::Clock clock;

    void clearPath();
    // void addPoint(double x, double y, double phi = 0.0);
    void addPoint(tuple<double, double, double> point);
    vector<geometry_msgs::msg::PoseStamped> pathToPoseStamped();
    vector<tuple<double, double, double>> PoseStampedToPath(vector<geometry_msgs::msg::PoseStamped> poses);

    vector<tuple<double, double, double>> path;
    static int mapOriginX;
    static int mapOriginY;
    static double mapResolution;
    

};

#endif
