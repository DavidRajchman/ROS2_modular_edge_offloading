#include "PathMessage.hpp"


PathMessage::PathMessage(rclcpp::Logger logger) : logger(logger) {}
PathMessage::PathMessage(rclcpp::Logger logger,  rclcpp::Publisher<nav_msgs::msg::Path>::SharedPtr publisher) : logger(logger), publisher(publisher), clock(RCL_SYSTEM_TIME) {}



void PathMessage::publishMsg(){
   nav_msgs::msg::Path msg;

   msg.header.stamp = clock.now();
   msg.header.frame_id="map";

   msg.poses = pathToPoseStamped();

   if( publisher == nullptr){
        RCLCPP_WARN(logger, "publisher is not initiated");
        return;
   } 
   publisher -> publish(msg);
}

void PathMessage::publishMsg(const nav_msgs::msg::Path::SharedPtr message){
    receiveMsg(message);
    publishMsg();
}

void PathMessage::receiveMsg(const nav_msgs::msg::Path::SharedPtr message){
    path = PoseStampedToPath(message->poses);
}

vector<tuple<double, double, double>>PathMessage::getPath(){
    return path;
}

vector<tuple<int, int, int>>PathMessage::getPathGrid(){
    vector<tuple<int, int, int>> pathGrid;
    for(tuple<double, double, double> point : path){
      tuple<int, int, int> pointGrid =positionToGridPosition(get<0>(point), get<1>(point), get<2>(point), mapOriginX, mapOriginY, mapResolution);
      pathGrid.push_back(pointGrid);
    }
    return pathGrid;
}

void PathMessage::setPath(vector<tuple<double, double, double>> path){
    clearPath();
    for (tuple<double, double, double> point: path){
        addPoint(point);
    }

}
void PathMessage::setPath(vector<tuple<int, int, int>> pathGrid, int mapOriginX, int mapOriginY, double resolution){
    clearPath();
    vector<tuple<double, double, double>> path;
    for (tuple<int, int, int> pointGrid: pathGrid){
        tuple<double, double, double> point = gridPositionToPosition(
            get<0>(pointGrid),get<1>(pointGrid),get<2>(pointGrid),
            mapOriginX, mapOriginY, resolution);
        path.push_back(point);
    }
    setMapDimensions(mapOriginX, mapOriginY,resolution);
    setPath(path);
}


void PathMessage::clearPath(){
    path.clear();
}

void PathMessage::addPoint(tuple<double, double, double> point){
    path.push_back(point);
}

void PathMessage::setMapDimensions(int mapOriginX, int mapOriginY, double mapResolution){
    this->mapOriginX = mapOriginX;
    this->mapOriginY = mapOriginY;
    this->mapResolution = mapResolution;
}

vector<geometry_msgs::msg::PoseStamped> PathMessage::pathToPoseStamped () {
    RCLCPP_INFO(logger, "Grid to poses");
    
    vector<geometry_msgs::msg::PoseStamped> poses;

    for(tuple<double, double, double> point : path){
        // RCLCPP_DEBUG_STREAM(get_logger(), "convert x: "<<gridPose.first<<" y: "<<gridPose.second);
        
        geometry_msgs::msg::PoseStamped pose;
        pose.pose.position.x = get<0>(point);
        pose.pose.position.y = get<1>(point);
        pose.pose.position.z = 0.0;

        auto [x,y,z,w] = yawToQuaternion(get<2>(point));
        pose.pose.orientation.x = x;
        pose.pose.orientation.y = y;
        pose.pose.orientation.z = z;
        pose.pose.orientation.w = w;

        poses.push_back(pose);

        // RCLCPP_DEBUG_STREAM(get_logger(), "map x: "<< gridPose.first<< " map y: " << gridPose.second );
        // RCLCPP_DEBUG_STREAM(get_logger(), "x: "<< pose.pose.position.x << "y: " << pose.pose.position.y );
    }
    return poses;
}

vector<tuple<double, double, double>> PathMessage::PoseStampedToPath(vector<geometry_msgs::msg::PoseStamped> poses) {
    RCLCPP_INFO(logger, "Poses to Grid");
    
    vector<tuple<double, double, double>> simplePoses;

    // for(int i =0 ; i< static_cast<int>)
    for(geometry_msgs::msg::PoseStamped point : poses){
        
        double x = point.pose.position.x;
        double y = point.pose.position.y;
        double yaw = quaternionToYaw(
            point.pose.orientation.x,
            point.pose.orientation.y,
            point.pose.orientation.z,
            point.pose.orientation.w
        );
        simplePoses.push_back({x,y,yaw});
    }
    return simplePoses;
}




