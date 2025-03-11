#include "MarkerMessage.hpp"

using namespace std;

MarkerMessage::MarkerMessage(rclcpp::Logger logger) : logger(logger){}
MarkerMessage::MarkerMessage(rclcpp::Logger logger, rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr publisher) : logger(logger), publisher(publisher), clock(RCL_SYSTEM_TIME){}

void MarkerMessage::setColor(double red, double green, double blue, double alpha){
    this -> red = red;
    this -> green = green;
    this -> blue = blue;
    this -> alpha = alpha;
}

void MarkerMessage::setScale(double x, double y, double z){
    scaleX = x;
    scaleY = y;
    scaleZ = z;
}

void MarkerMessage::setID(int id){
    this -> id = id; 
}

int MarkerMessage::getID(){
    return id;
}

void MarkerMessage::setType(string action){
    if (action == "add")
        this -> action = 0;
    else if (action == "modify")
        this -> action = 0;
    else if (action == "delete")
        this -> action = 2;
    else if (action == "delete all")
        this -> action = 3;
    else
        RCLCPP_WARN(logger, "invalid action of marker");
}


void MarkerMessage::setPosition(tuple<int, int, int> poseGrid, int mapOriginX, int mapOriginY, double mapResolution){
    tuple<double, double, double> pose = gridPositionToPosition(
        get<0>(poseGrid), get<1>(poseGrid), get<2>(poseGrid),
        mapOriginX, mapOriginY, mapResolution
    );
    setPosition(pose);
}

void MarkerMessage::setPosition(tuple<double, double, double> pose){
    x = get<0>(pose);
    y = get<1>(pose);
    phi = get<2>(pose);
}

void MarkerMessage::receiveMsg(visualization_msgs::msg::Marker::SharedPtr message){
    x = message -> pose.position.x;
    y = message -> pose.position.y;
    phi = quaternionToYaw(
        message -> pose.orientation.x,
        message -> pose.orientation.y,
        message -> pose.orientation.z,
        message -> pose.orientation.w
    );
}

void MarkerMessage::publishMsg(visualization_msgs::msg::Marker::SharedPtr message){
    receiveMsg(message);
    publishMsg();
}

void MarkerMessage::publishMsg(){
    visualization_msgs::msg::Marker msg;

    msg.header.frame_id = "map";  // Rámec (frame) pro zobrazení
    msg.header.stamp = clock.now();
    msg.ns = "marker_space";  // Jméno prostoru
    msg.id = id++;  // ID markeru
    msg.type = visualization_msgs::msg::Marker::ARROW;  // Typ markeru (šipka)
    msg.action = action;  // Akce (přidání)

    // Nastavení pozice markeru
    msg.pose.position.x = x;
    msg.pose.position.y = y;
    msg.pose.position.z = 0.0;

    tuple<double, double, double, double> quaternion = yawToQuaternion(phi);
    msg.pose.orientation.x = get<0>(quaternion);
    msg.pose.orientation.y = get<1>(quaternion);
    msg.pose.orientation.z = get<2>(quaternion);
    msg.pose.orientation.w = get<3>(quaternion);

    // Nastavení měřítek
    msg.scale.x = scaleX;  // Délka šipky
    msg.scale.y = scaleY;  // Tloušťka šipky
    msg.scale.z = scaleZ;  // Výška šipky

    // Nastavení barvy (RGBA)
    msg.color.r = red;  // Červená
    msg.color.g = green;  // Zelená
    msg.color.b = blue;  // Modrá
    msg.color.a = alpha;  // Průhlednost

    // Publikování markeru
    publisher->publish(msg);
}
