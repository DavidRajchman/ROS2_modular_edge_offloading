#include "ROS2_math.hpp"

char clamp99_99(int value){
    if(value >=100)
        return 99;
    else if(value <=-100)
        return -99;
    else
        return value;
}

char clamp99_99(char value){
    return clamp99_99(static_cast<int>(value));
}



char clamp0_99(int value){
    if(value >=100)
        return 99;
    else if(value <0)
        return 0;
    else
        return value;
}

char clamp0_99(char value){
    return clamp0_99(static_cast<int>(value));
}

double quaternionToYaw(double x, double y, double z, double w){

    tf2::Quaternion q(x,y,z,w);
    tf2::Matrix3x3 m(q);

    double roll, pitch, yaw;
    m.getRPY(roll, pitch, yaw);
    return yaw;
}

pair<int, int> yawToGridDirection(double angle){
    //
    vector<pair<int,int>> moves = {{-1,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,0}};
    //bias to <0, 2PI>
    angle = angle + M_PI;

    //normalization
    angle = angle / (2*M_PI);

    // to interval <0,8>
    angle *= 8;

    int index = round(angle);
    index = index % 8;

    return moves[index];
}

int yawToGridIndex(double angle){
    //
    vector<pair<int,int>> moves = {{-1,0}, {-1,-1}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,0}};
    //bias to <0, 2PI>
    angle = angle + M_PI;

    //normalization
    angle = angle / (2*M_PI);

    // to interval <0,8>
    angle *= 8;

    int index = round(angle);
    index = index % 8;

    return index;
}
