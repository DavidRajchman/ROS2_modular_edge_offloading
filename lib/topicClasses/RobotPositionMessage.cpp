#include "RobotPositionMessage.hpp"


RobotPositionMessage::RobotPositionMessage(rclcpp::Logger logger) : logger(logger) {}
RobotPositionMessage::RobotPositionMessage(rclcpp::Logger logger,  rclcpp::Publisher<services::msg::Position>::SharedPtr publisher) : logger(logger), publisher(publisher) {}

void RobotPositionMessage::setPosition(double poseX, double poseY, double posePhi){
    setPositionX(poseX);
    setPositionY(poseY);
    setPositionPhi(posePhi);
}

void RobotPositionMessage::setPositionX(double poseX){
    this -> poseRobX = poseX;
}

void RobotPositionMessage::setPositionY(double poseY){
    this -> poseRobY = poseY;
}

void RobotPositionMessage::setPositionPhi(double posePhi){
    if(posePhi <= -M_PI || posePhi >= M_PI){
        RCLCPP_WARN_STREAM(logger, "Invalid argument of phi: "<<posePhi);
        return;
    }
    this-> poseRobPhi = posePhi;
}

tuple<double, double, double> RobotPositionMessage::getPosition(){
    return {poseRobX, poseRobY, poseRobPhi};
}


//position in grid
void RobotPositionMessage::setMapPosition(int mapX, int mapY, int indexDirection){
    setMapOriginX(mapX);
    setMapOriginY(mapY);
    setIndexDirecion(indexDirection);
}

void RobotPositionMessage::setMapX(int mapX){
    if (mapX < 0){
        RCLCPP_WARN_STREAM(logger, "Invalid argument of mapX: "<<mapX);
        return;
    }
    else if(mapSizeX != -1 && mapSizeX <= mapX){
        RCLCPP_WARN_STREAM(logger, "Invalid argument of mapX: "<<mapX);
        return;
    }
    this -> mapRobX = mapX;
}
void RobotPositionMessage::setMapY(int mapY){
    if (mapY < 0){
        RCLCPP_WARN_STREAM(logger, "Invalid argument of mapY: "<<mapY);
        return;
    }
    else if(mapSizeY != -1 && mapSizeY <= mapY){
        RCLCPP_WARN_STREAM(logger, "Invalid argument of mapY: "<<mapY);
        return;
    }
    this -> mapRobY = mapY;
}

void RobotPositionMessage::setIndexDirecion(int indexDirection){
    if(indexDirection < 0 || indexDirection >= 8){
        RCLCPP_WARN_STREAM(logger, "Invalid argument of indexPhi: "<<indexDirection);
        return;
    }
    this -> indexPhi = indexDirection;
}

tuple<int, int, int> RobotPositionMessage::getMapPosition(){
    return {mapRobX, mapRobY, indexPhi};
}


//map origin in grid
void RobotPositionMessage::setMapOrigin(int originX, int originY){
    setMapOriginX(originX);
    setMapOriginY(originY);
}


void RobotPositionMessage::setMapOriginX(int originX){
    this -> mapOriginX = originX;
}

void RobotPositionMessage::setMapOriginY(int originY){
    this -> mapOriginY = originY;
}

pair<int, int> RobotPositionMessage::getMapOrigin(){
    return make_pair(mapOriginX, mapOriginY);
}

void RobotPositionMessage::setMapDimensionx(int sizeX, int sizeY){
    this -> mapSizeX = sizeX;
    this -> mapSizeY = sizeY;
}


//publishing and receiving messages
void RobotPositionMessage::publishRobotPosition(){
    
    msg.pose_rob_x = poseRobX;
    msg.pose_rob_y = poseRobY;
    msg.pose_rob_phi = poseRobPhi;

    msg.map_rob_x = mapRobX;
    msg.map_rob_y = mapRobY;
    msg.map_phi_index = indexPhi;

    msg.map_origin_x = mapOriginX;
    msg.map_origin_y = mapOriginY;

    publisher -> publish(msg);
}

void RobotPositionMessage::publishRobotPosition(services::msg::Position::SharedPtr message){
    receiveRobotPosition(message);
    publishRobotPosition();
}

void RobotPositionMessage::receiveRobotPosition(services::msg::Position::SharedPtr message){
    setPosition(message->pose_rob_x, message->pose_rob_y, message->pose_rob_phi);
    setMapPosition(message->map_rob_x, message->map_rob_y, message -> map_phi_index);
    setMapOrigin(message-> map_origin_x, message-> map_origin_x);
}







