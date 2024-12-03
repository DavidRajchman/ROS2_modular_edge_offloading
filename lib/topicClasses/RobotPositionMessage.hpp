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
    void setPositionX(double poseX);
    void setPositionY(double poseY);
    void setPositionPhi(double posePhi);
    tuple<double, double, double> getPosition();


    //position in grid
    void setMapPosition(int mapX, int mapY, int indexDirection);
    void setMapX(int mapX);
    void setMapY(int mapY);
    void setIndexDirecion(int indexDirection);
    tuple<int, int, int> getMapPosition();


    //map origin in grid
    void setMapOrigin(int originX, int originY);
    void setMapOriginX(int originX);
    void setMapOriginY(int originY);
    pair<int, int> getMapOrigin();

    //map dimensions
    void setMapDimensionx(int sizeX, int sizeY);

    //publishing and receiving messages
    void publishRobotPosition();
    void publishRobotPosition(services::msg::Position::SharedPtr message);
    void receiveRobotPosition(services::msg::Position::SharedPtr message);

private:
    rclcpp::Logger logger ;
    rclcpp::Publisher<services::msg::Position>::SharedPtr publisher = nullptr;
    services::msg::Position msg;


    //position of robot [m]
    double poseRobX;
    double poseRobY;
    double poseRobPhi;

    //position of robot in grid [cells]
    int mapRobX;
    int mapRobY;
    int indexPhi;

    //map origin in grid [cells]
    int mapOriginX;
    int mapOriginY;

    //dimensions of map
    int mapSizeX = -1;
    int mapSizeY = -1;



};

#endif
