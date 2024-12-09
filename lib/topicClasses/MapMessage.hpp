#ifndef MAP_MESSAGE
#define MAP_MESSAGE

#include <rclcpp/rclcpp.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <cmath>
#include "../math/ROS2_math.hpp"

using namespace std;


class MapMessage {
public:
    MapMessage(rclcpp::Logger logger);
    MapMessage(rclcpp::Logger logger, rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr publisher);

    //map origin in grid
    void setMapOrigin(double originX, double originY, double originPhi);
    tuple<double, double, double> getMapOrigin();
    tuple<int, int, int> getMapOriginGrid();

    //map dimensions
    void setMapDimensions(int sizeX, int sizeY, double resolution);
    tuple<int, int , double> getMapDimensions();

    void setMap(vector<int8_t> inputMap);
    vector<int8_t> getMap();

    //publishing and receiving messages
    void publishMsg();
    void publishMsg(const nav_msgs::msg::OccupancyGrid::SharedPtr message);
    void receiveMsg(const nav_msgs::msg::OccupancyGrid::SharedPtr message);

private:
    rclcpp::Logger logger;
    rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr publisher = nullptr;
    nav_msgs::msg::OccupancyGrid msg;
    rclcpp::Clock clock;


    //setters map origin
    void setOriginX(double originX);
    void setOriginY(double originY);
    void setOriginPhi(double originPhi);


    //map origin [m]
    double originX;
    double originY;
    double originPhi;


    //dimensions of map
    int mapSizeX;
    int mapSizeY;
    double mapResolution;

    vector<int8_t> map;
};

#endif
