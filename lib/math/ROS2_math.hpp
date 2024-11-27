#ifndef ROS2_MATH
#define ROS2_MATH

#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2_ros/buffer.h>
// #include <iostream>

using namespace std;

char clamp99_99(int value);
char clamp99_99(char value);

char clamp0_99(int value);
char clamp0_99(char value);

double quaternionToYaw(double x, double y, double z, double w);

pair<int, int> yawToGridDirection(double angle);
int yawToGridIndex(double angle);


#endif