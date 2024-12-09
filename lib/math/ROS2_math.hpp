#ifndef ROS2_MATH
#define ROS2_MATH

#include <tf2/LinearMath/Quaternion.h>
#include <tf2/LinearMath/Matrix3x3.h>
#include <tf2_ros/buffer.h>
#include <tf2/LinearMath/Quaternion.h>
#include <opencv2/opencv.hpp>


using namespace std;

char clamp99_99(int value);
char clamp99_99(char value);

char clamp0_99(int value);
char clamp0_99(char value);

double clampPI_PI(double value);
double clampPI_PI(float value);

double quaternionToYaw(double x, double y, double z, double w);
tuple<double, double, double, double> yawToQuaternion(double yaw);

pair<int, int> yawToGridDirection(double angle);
int yawToGridIndex(double angle);
double gridIndexToYaw(int index);

tuple<double, double, double> gridPositionToPosition(int mapX, int mapY, int indexPhi, int originX, int originY, double resolution);
tuple<int, int, int> positionToGridPosition(double x, double y, double phi, int mapOriginX, int mapOriginY, double resolution);

vector<vector<bool>> createDelatatedMap(vector<int8_t>& mapMsg, int sizeX, int sizeY, int dilatation = 5, int obstacleLimit = 80);

#endif