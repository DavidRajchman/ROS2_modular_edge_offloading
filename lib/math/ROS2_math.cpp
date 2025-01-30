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

double clampPI_PI(double value){
    while (value <= -M_PI || value >= M_PI) {
        if (value <= -M_PI) {
            value += 2 * M_PI;
        } else {
            value -= 2 * M_PI;
        }
    }
    return value;
}
double clampPI_PI(float value){
    while (value <= -M_PI || value >= M_PI) {
        if (value <= -M_PI) {
            value += 2 * M_PI;
        } else {
            value -= 2 * M_PI;
        }
    }
    return value;
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

double gridIndexToYaw(int index){
    return (index * M_PI / 4) - M_PI;
}

vector<vector<bool>> dilatation(vector<int8_t> data, int sizeX, int sizeY, int DILATATION, int obstacleLimit) {
    //create 2D boolean dilatated array map from 1D data (map data from ROS msg)

    //creating cv matrix suitable for dilatation
    cv::Mat image(sizeY, sizeX, CV_8U, data.data());

    // creating kernel for dilatation
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(DILATATION, DILATATION));

    // application of dilatation
    cv::Mat dilated;
    cv::dilate(image, dilated, kernel);

    // print result
    // cout << "Původní matice:\n" << image << "\n\n";
    // cout << "Matice po dilataci:\n" << dilated << "\n";
    
    //converting to boolean 2D array
    vector<vector<bool>> dilatatedGrid;
    dilatatedGrid.resize(sizeX, std::vector<bool>(sizeY,false));
    for (int x =0; x<sizeY;x++){
        for (int y=0; y<sizeX;y++){
            dilatatedGrid[y][x] = dilated.at<unsigned char>(x,y)<=obstacleLimit ? false : true; 
        }
    }
    return dilatatedGrid;
}



tuple<double, double, double> gridPositionToPosition(int mapX, int mapY, int indexPhi, int mapOriginX, int mapOriginY, double resolution){
    double x = (mapX - mapOriginX) * resolution;
    double y = (mapY - mapOriginY) * resolution;
    double phi = gridIndexToYaw(indexPhi);
    return {x, y, phi};
}

tuple<int, int, int> positionToGridPosition(double x, double y, double phi, int mapOriginX, int mapOriginY, double resolution){
    int mapX = round(x / resolution) + mapOriginX;
    int mapY = round(y / resolution) + mapOriginY;
    int indexPhi = yawToGridIndex(phi);
    return {mapX, mapY, indexPhi};
}

void print2DArray(vector<vector<bool>> array){
    cout << "printed array:" << endl;
    cout << "size x:"<< static_cast<int>(array.size())<<" size y: "<< static_cast<int>(array[0].size()) << "\n";
    for(int x =0; x < static_cast<int>(array.size()); x++ ){
        cout<<"line "<< x <<": [";
        for(int y = 0; y < static_cast<int>(array[0].size()); y++ ){
            cout << static_cast<int>(array[x][y]) << (y != (static_cast<int>(array[0].size()-1))? ", ":"" );
        }
        cout<<"]\n";
    } 
}