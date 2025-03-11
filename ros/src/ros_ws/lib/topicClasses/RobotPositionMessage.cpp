#include "RobotPositionMessage.hpp"


RobotPositionMessage::RobotPositionMessage(rclcpp::Logger logger) : logger(logger) {}
RobotPositionMessage::RobotPositionMessage(rclcpp::Logger logger,  rclcpp::Publisher<services::msg::Position>::SharedPtr publisher) : logger(logger), publisher(publisher) {}

void RobotPositionMessage::setPosition(double poseX, double poseY, double posePhi){
    setPositionX(poseX);
    setPositionY(poseY);
    setPositionPhi(posePhi);
    setMapPosition(poseX, poseY, posePhi);
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
void RobotPositionMessage::setMapPosition(double poseX, double poseY, double posePhi){
    
    if(mapResolution == -1 ){
        RCLCPP_WARN_ONCE(logger, "Not initiated map resolution");
        return;
    }
    else if (mapOriginX == -1){
        RCLCPP_WARN(logger, "Not initiated map origin x");
        return;
    }
    else if (mapOriginY == -1){
        RCLCPP_WARN(logger, "Not initiated map origin y");
        return;
    }
    // RCLCPP_DEBUG_STREAM(logger, "x: "<<poseX<<" y: "<<poseY<<" phi: "<<posePhi );
    // RCLCPP_DEBUG_STREAM(logger, "orinx: "<<mapOriginX<<" oriny: "<<mapOriginY<<" res: "<<mapResolution );
    auto [mapX, mapY, indexPhi] =positionToGridPosition(poseX, poseY, posePhi, mapOriginX, mapOriginY, mapResolution);
    setMapPosition(mapX, mapY, indexPhi);
}

void RobotPositionMessage::setMapPosition(int mapX, int mapY, int indexDirection){
    // RCLCPP_DEBUG_STREAM(logger, "mapx: "<<mapX<<" mapY: "<<mapY<<" phi: "<<indexDirection );
    setMapX(mapX);
    setMapY(mapY);
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
    RCLCPP_DEBUG_STREAM_ONCE(logger, "robot position: setting origin x: "<<mapOriginX);

}

void RobotPositionMessage::setMapOriginY(int originY){
    this -> mapOriginY = originY;
    RCLCPP_DEBUG_STREAM_ONCE(logger, "robot position: setting origin y: "<<mapOriginY);
}

pair<int, int> RobotPositionMessage::getMapOrigin(){
    return make_pair(mapOriginX, mapOriginY);
}

void RobotPositionMessage::setMapDimensions(int sizeX, int sizeY,double resolution){
    this -> mapSizeX = sizeX;
    this -> mapSizeY = sizeY;
    this -> mapResolution = resolution;
    RCLCPP_DEBUG_STREAM_ONCE(logger, "robot position: setting dimensions x: "<<mapSizeX<<" mapsizeY: "<<mapSizeY<<" map resolution: "<<mapResolution );
}


//publishing and receiving messages
void RobotPositionMessage::publishMsg(){
    if (publisher == nullptr){
        RCLCPP_ERROR(logger, "class doesn't have a publisher");
        return;
        
    }
    
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

void RobotPositionMessage::publishMsg(services::msg::Position::SharedPtr message){
    receiveMsg(message);
    publishMsg();
}

void RobotPositionMessage::receiveMsg(services::msg::Position::SharedPtr message){
    poseRobX = message->pose_rob_x;
    poseRobY = message->pose_rob_y;
    poseRobPhi = message->pose_rob_phi;

    mapRobX = message->map_rob_x;
    mapRobY = message->map_rob_y;
    indexPhi = message->map_phi_index;

    mapOriginX = message->map_origin_x;
    mapOriginY = message->map_origin_y;

    
    // setMapOrigin(message-> map_origin_x, message-> map_origin_y);
    // setPosition(message->pose_rob_x, message->pose_rob_y, message->pose_rob_phi);
    // setMapPosition(message->map_rob_x, message->map_rob_y, message -> map_phi_index);
}







