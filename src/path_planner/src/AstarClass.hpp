#ifndef ASTAR_CLASS
#define ASTAR_CLASS

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <visualization_msgs/msg/marker_array.hpp>
#include <nav_msgs/msg/path.hpp>
#include <sensor_msgs/msg/laser_scan.hpp>
#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <services/srv/base_angle.hpp>
#include <services/srv/rotation_lidar.hpp>
#include <services/msg/position.hpp>
#include <services/msg/control_node.hpp>
#include <services/msg/status_node.hpp>
#include <Eigen/Dense>
#include <queue>
#include <iostream>
#include <vector>
#include <opencv2/opencv.hpp>


using namespace std;

struct NodeStar {
    double cost;
    int x;
    int y;
    int prevX;
    int prevY;
    int lastChangeDir;

    bool operator>(const NodeStar& other) const {
        return cost > other.cost;
    }
};


class Astar : public rclcpp::Node {
public:
    Astar();

private:
    //parameters from yaml file
    double CAR_WIDTH, LOOK_DISTANCE, PENALTY_CHANGE_HIGH, PENALTY_CHANGE_LOW, PENALTY_INPUT_OUTPUT, PENALTY_LAST_CHANGE;
    int DILATATION;
    
    //parameters from messages
    unsigned int widthMap, heightMap;
    float mapRes;
    int mapRobX, mapRobY;
    int mapOriginX, mapOriginY;
    float poseRobX, poseRobY, poseRobPhi;
    vector<vector<int8_t>> grid;
    vector<vector<bool>> gridDil;
    double goalX, goalY, goalPhi;
    pair<int, int> mapGoalPhi;
    int mapGoalX, mapGoalY;
    vector<pair<int,int>> path;
    vector<array<double, 3>> checkpoints;
    double originX, originY, originPhi;

    // Publishers and Subscribers
    rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr path_pub;

    rclcpp::Subscription<nav_msgs::msg::OccupancyGrid>::SharedPtr map_sub;
    rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr goal_sub;
    rclcpp::Subscription<services::msg::Position>::SharedPtr pose_sub;
    
    //control params
    bool closedPath = true;

    //callbacks
    void mapCb(const nav_msgs::msg::OccupancyGrid::SharedPtr msg);
    void poseCb(const services::msg::Position::SharedPtr msg);
    void goalCb(const geometry_msgs::msg::PoseStamped::SharedPtr msg);
    
    //
    void publishPath();
    pair<int, int> yawToGridDirection(double angle);
    void createDelatatedMap(vector<int8_t>& mapMsg);
    vector<geometry_msgs::msg::PoseStamped>convertGridPathToPoses(const vector<pair<int, int>> path);
    double heuristic (const std::pair<int, int>& node, const std::pair<int, int>& goal);
    void makePath(const vector<vector<std::array<int, 2>>>& way,const pair<int, int>& start, const pair<int, int>& goal);
    // vector<NodeStar> getNeighbor(const NodeStar& node, const vector<vector<int8_t>>& grid, vector<vector<array<int, 2>>>& way, 
    //                     const pair<int, int>& start, const tuple<int, int, double>& goal, int lastChangeDir);
    vector<NodeStar> getNeighbor(const NodeStar& node, const vector<vector<bool>>& grid, vector<vector<array<int, 2>>>& way, 
                        const tuple<int, int, pair<int,int>>& goal, int lastChangeDir);
    void astar(const vector<vector<bool>> grid, const pair<int, int>& start, 
                                    const tuple<int, int, pair<int,int>>& goal);     
    // vector<pair<int, int>> astar(const vector<vector<int8_t>> grid,  
    //                                 const tuple<int, int, double>& goal); 

    double quaternionToYaw(double x, double y, double z, double w);
};
#endif