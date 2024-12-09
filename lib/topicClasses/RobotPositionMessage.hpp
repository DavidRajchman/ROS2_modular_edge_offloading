#ifndef ROBOT_POSITION_MESSAGE
#define ROBOT_POSITION_MESSAGE

#include <rclcpp/rclcpp.hpp>
#include "services/msg/position.hpp"
#include <cmath>
#include "../math/ROS2_math.hpp"

using namespace std;


class RobotPositionMessage {
public:
    RobotPositionMessage(rclcpp::Logger logger);
    RobotPositionMessage(rclcpp::Logger logger, rclcpp::Publisher<services::msg::Position>::SharedPtr publisher);

    //position in meters
    void setPosition(double poseX, double poseY, double posePhi);
    tuple<double, double, double> getPosition();

    tuple<int, int, int> getMapPosition();


    //map origin in grid
    void setMapOrigin(int originX, int originY);
    pair<int, int> getMapOrigin();

    //map dimensions
    void setMapDimensions(int sizeX, int sizeY, double resolution);

    //publishing and receiving messages
    void publishMsg();
    void publishMsg(services::msg::Position::SharedPtr message);
    void receiveMsg(services::msg::Position::SharedPtr message);

private:
    rclcpp::Logger logger;
    rclcpp::Publisher<services::msg::Position>::SharedPtr publisher = nullptr;
    services::msg::Position msg;

    //setters position [m]
    void setPositionX(double poseX);
    void setPositionY(double poseY);
    void setPositionPhi(double posePhi);

    //setters grid position [cell]
    void setMapPosition(double poseX, double poseY, double posePhi);
    void setMapPosition(int mapX, int mapY, int indexDirection);
    void setMapX(int mapX);
    void setMapY(int mapY);
    void setIndexDirecion(int indexDirection);

    //setters map origin
    void setMapOriginX(int originX);
    void setMapOriginY(int originY);


    //position of robot [m]
    double poseRobX;
    double poseRobY;
    double poseRobPhi;

    //position of robot in grid [cells]
    int mapRobX;
    int mapRobY;
    int indexPhi;

    //map origin in grid [cells]
    int mapOriginX = -1;
    int mapOriginY = -1;

    //dimensions of map
    int mapSizeX = -1;
    int mapSizeY = -1;
    int mapResolution = -1;
};

#endif
