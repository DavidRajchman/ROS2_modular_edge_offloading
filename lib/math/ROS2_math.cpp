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