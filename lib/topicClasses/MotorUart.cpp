#include "MotorUart.hpp"


MotorUart::MotorUart(rclcpp::Logger logger) : logger(logger) {
    // cout<<"hi"<<endl;
    // this -> logger = logger;
    setConnection("/dev/teensy");
}


void MotorUart::setSpeed(char speed){
    speed = clamp99_99(speed);
    this -> speed = speed;
}

char MotorUart::getSpeed(){
    return speed;
}


void MotorUart::setSteer(char steer){
    steer = clamp99_99(steer);
    this -> steer = steer;
}

char MotorUart::getSteer(){
    return steer;
}

void MotorUart::setMode(char mode){
    if (mode <0 || mode>1){
        RCLCPP_WARN_STREAM(logger, "invalid arguemnt mode: " << mode);
        return;
    }
    this -> mode = mode;
}

char MotorUart::getMode(){
    return mode;
}

void MotorUart::setLidarSpeed(char speed){
    speed = clamp0_99(speed);
    this -> lidarSpeed = speed;
}

char MotorUart::getLidarSpeed(){
    return lidarSpeed;
}

void MotorUart::publishControl(){
    
    string strSpeed= to_string(getSpeed());
    string strSteer= to_string(getSteer());
    char command[8] ;

    command[0] = 'c';
    command[1] = speed >=0 ? '+' : '-';
    command[2] = (speed <=-10 || speed >=10) ? strSpeed[strSpeed.size()-2] : '0';
    command[3] = strSpeed[strSpeed.size()-1];
    command[4] = steer >=0 ? '+' : '-';
    command[5] = (steer <=-10 || steer >=10) ? strSteer[strSteer.size()-2] : '0';
    command[6] = strSteer[strSteer.size()-1];
    command[7] = '\n';

    write(serial_port, command, sizeof(command));

}

void MotorUart::publishControl(char speed, char steer){
    setSpeed(speed);
    setSteer(steer);
    publishControl();
}

void MotorUart::publishLidarSpeed(){
    string strSpeed= to_string(getLidarSpeed());
    char command[4] ;

    command[0] = 'l';
    command[1] = (speed <=-10 || speed >=10) ? strSpeed[strSpeed.size()-2] : '0';
    command[2] = strSpeed[strSpeed.size()-1];
    command[3] = '\n';

    write(serial_port, command, sizeof(command));
}
void MotorUart::publishLidarSpeed(char speed){
    setLidarSpeed(speed);
    publishLidarSpeed();
}

void MotorUart::publishMode(){
    string mode= to_string(getMode());
    char command[3] ;

    command[0] = 'm';
    command[1] = mode[mode.size()-1];
    command[2] = '\n';

    write(serial_port, command, sizeof(command));
} 

void MotorUart::publishMode(char mode){
    setMode(mode);
    publishMode();
} 


void MotorUart::setConnection(string dev){
    const char* cdev = dev.c_str();
// void MotorUart::setConnection(){
    struct termios tty;
    serial_port = open(cdev, O_RDWR); //TODO jetson

    if (tcgetattr(serial_port, &tty) != 0) {
        string err = strerror(errno);
        // status = Status::ERROR;
        RCLCPP_ERROR(logger, "Error %i from tcgetattr: %s", errno, err.c_str());
    } else {
        RCLCPP_WARN(logger, "Communication with Arduino OK");
    }

    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~(OPOST | ONLCR);
    tty.c_cc[VTIME] = 10;
    tty.c_cc[VMIN] = 0;
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {
        string err = strerror(errno);
        // status = Status::ERROR;
        RCLCPP_ERROR(logger, "Error %i from tcsetattr: %s", errno, err.c_str());
    }
    sleep(2);
    RCLCPP_INFO(logger, "Connection established");

}







