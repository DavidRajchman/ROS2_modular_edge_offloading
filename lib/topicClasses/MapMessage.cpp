#include "MapMessage.hpp"


MapMessage::MapMessage(rclcpp::Logger logger) : logger(logger) {}
MapMessage::MapMessage(rclcpp::Logger logger,  rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr publisher) : logger(logger), publisher(publisher), clock(RCL_SYSTEM_TIME) {}


void MapMessage::setMapOrigin(double originX, double originY, double originPhi){
    setOriginX(originX);
    setOriginY(originY);
    setOriginPhi(originPhi);
}

tuple<double, double, double> MapMessage::getMapOrigin(){
    return {originX, originY, originPhi};
}

tuple<int, int, int> MapMessage::getMapOriginGrid(){
    // mapOriginX = round(-originX / mapResolution);
    // mapOriginY = round(-originY / mapResolution);
    return positionToGridPosition(-originX, -originY, originPhi, 0, 0, mapResolution);
}


//map dimensions
void MapMessage::setMapDimensions(int sizeX, int sizeY, double resolution){
    this -> mapSizeX = sizeX ;
    this -> mapSizeY = sizeY ;
    this -> mapResolution = resolution;
}

tuple<int, int , double> MapMessage::getMapDimensions(){
    return {mapSizeX, mapSizeY, mapResolution};
}


//publishing and receiving messages
void MapMessage::publishMsg(){
    nav_msgs::msg::OccupancyGrid message;

    message.header.stamp = clock.now();
    message.info.resolution = mapResolution;
    message.info.width = mapSizeX;
    message.info.height = mapSizeY;

    message.info.origin.position.x = originX;
    message.info.origin.position.y = originY;
    message.info.origin.position.z = 0.0;

    // double x,y,z,w;
    auto [x,y,z,w] = yawToQuaternion(originPhi);
    message.info.origin.orientation.x = x; 
    message.info.origin.orientation.y = y; 
    message.info.origin.orientation.w = w; 
    message.info.origin.orientation.z = z; 

    message.data = map;

    publisher -> publish(message);
     
}

void MapMessage::publishMsg(nav_msgs::msg::OccupancyGrid::SharedPtr message){
    receiveMsg(message);
    publishMsg();
}

void MapMessage::receiveMsg(nav_msgs::msg::OccupancyGrid::SharedPtr message){
    double phi = quaternionToYaw(
        message->info.origin.orientation.x,
        message->info.origin.orientation.y,
        message->info.origin.orientation.z,
        message->info.origin.orientation.w
    );
    
    setMapOrigin(
        message->info.origin.position.x,
        message->info.origin.position.y,
        phi
    );

    setMapDimensions(
        message-> info.width, 
        message-> info.height,
        message-> info.resolution
    );

    setMap( message -> data);

}

void MapMessage::setOriginX(double originX){
    this -> originX = originX;
}

void MapMessage::setOriginY(double originY){
    this -> originY = originY;
}

void MapMessage::setOriginPhi(double originPhi){
    if(originPhi < -M_PI || originPhi > M_PI){
        RCLCPP_WARN_STREAM(logger, "Invalid argument of origin phi: "<<originPhi);
        return;
    }
    this -> originPhi = originPhi;

}

void MapMessage::setMap(vector<int8_t> inputMap){
    this-> map = inputMap;
}

vector<int8_t> MapMessage::getMap(){
    return map;
}



