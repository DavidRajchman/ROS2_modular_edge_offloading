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


tuple<double, double, double, double> yawToQuaternion(double yaw)
{
    tf2::Quaternion q;
    q.setRPY(0.0, 0.0, yaw);
    return {q.x(), q.y(), q.z(), q.w()};
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


vector<vector<bool>> createDelatatedMap(vector<int8_t>& mapMsg, int sizeX, int sizeY, int dilatation, int obstacleLimit ){
    
    cv::Mat matInput(sizeX, sizeY, CV_8U, &mapMsg[0]);
    // creating kernel for dilatation
    cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, //shape of kernel ELLIPSE
                                                cv::Size(2 * dilatation + 1, 2 * dilatation + 1), // size of kernel
                                                cv::Point(dilatation, dilatation)); // definition of center

    // Aplikujeme dilataci
    cv::Mat dilatedImage;
    cv::dilate(matInput, dilatedImage, element);
    // RCLCPP_DEBUG_STREAM(get_logger(), "access: " << (int)dilatedImage.at<unsigned char>(0,0));
    
    // RCLCPP_INFO(get_logger(), "before mat to vector" );
    vector<vector<bool>> dilatatedGrid;
    dilatatedGrid.resize(sizeX, std::vector<bool>(sizeY));
    // vector<vector<bool>> dilatatedGridated(grid.size(), vector<bool>(grid[0].size(),false));
    for (int x =0; x<static_cast<int>(sizeX);x++){
        for (int y=0; y<static_cast<int>(sizeY);y++){
            // RCLCPP_DEBUG_STREAM(get_logger(), "transform : "<< x<<" "<<y);
            dilatatedGrid[x][y] = dilatedImage.at<unsigned char>(x,y)<=obstacleLimit ? false : true; 
        }
    }
    return dilatatedGrid;
    // RCLCPP_ERROR_STREAM(get_logger(), "zkouska dilatation: " << gridDil[70][11] ); //ano
    // RCLCPP_ERROR_STREAM(get_logger(), "zkouska dilatation: " << gridDil[70][12] ); //ne
}