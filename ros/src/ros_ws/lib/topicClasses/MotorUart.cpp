#include "MotorUart.hpp"


MotorUart::MotorUart(rclcpp::Logger logger) : logger(logger) {
    // cout<<"hi"<<endl;
    // this -> logger = logger;
    setConnection("/dev/teensy");
    

    // thread serialReader(readData);
    thread serialReader(bind(&MotorUart::readData, this));
    serialReader.detach();
}

MotorUart::~MotorUart() {
    if (serialPort.is_open()) {
        serialPort.close();
    }
}



void MotorUart::setSpeed(float speed){
    // speed = clamp99_99(speed);
    this -> speed = speed;
}

float MotorUart::getSpeed(){
    return speed;
}


void MotorUart::setSteer(float rotation){
    float angle = atan((rotation * BASE_WHEEL)/ speed);
    // steer = clamp99_99(steer);

    this -> steer = angle;
}

float MotorUart::getSteer(){
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
    
    // string strSpeed= to_string(getSpeed());
    // string strSteer= to_string(getSteer());

    char speedSign = speed>0 ? '+' : '-'; 
    char steerSign = steer>0 ? '+' : '-'; 
    
    serialPort << "c" 
                << speedSign
                << fixed << setprecision(2) << getSpeed()
                << steerSign 
                << fixed << setprecision(2) << getSteer();

    cout << "c" 
                << speedSign
                << fixed << setprecision(2) << getSpeed()
                << steerSign 
                << fixed << setprecision(2) << getSteer();
}

void MotorUart::publishControl(float speed, float steer){
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


void MotorUart::readData() {
    
    string receivedData;

    while (true) {
        getline(serialPort, receivedData);  // Čtení jedné řádky
        cout << "Přijato: " << receivedData << endl;
        this_thread::sleep_for(chrono::milliseconds(100));
    }
}


void MotorUart::setConnection(string dev){
    serialPort.open(dev, ios::in | ios::out);
    if (!serialPort.is_open()) {
        std::cerr << "ERROR: connection not established!" << std::endl;
        return;
    }
    sleep(2);
    RCLCPP_INFO(logger, "Connection established");
}







