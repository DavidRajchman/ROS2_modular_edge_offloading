#include "MotorUart.hpp"


MotorUart::MotorUart(rclcpp::Logger logger) : logger(logger) {
    // cout<<"hi"<<endl;
    // this -> logger = logger;
    // setConnection("/dev/teensy");
    if (!setConnection("/dev/Teensy")) return;

    

    // thread serialReader(readData);
    thread serialReader(bind(&MotorUart::readData, this));
    serialReader.detach();
}

MotorUart::~MotorUart() {
    // if (serialPort.is_open()) {
    //     serialPort.close();
    // }
    if (serialPort >= 0)
        close(serialPort);

}



void MotorUart::setSpeed(float speed){
    if (speed <0.35 && speed > -0.2){
        this -> speed = 0.0;
        return;
    } 
    // speed = clamp99_99(speed);
    this -> speed = speed;
}

float MotorUart::getSpeed(){
    return speed;
}


void MotorUart::setSteer(float rotation){
    if (speed <0.35 && speed > -0.2){
        this -> steer = 0.0;
        return;
    } 
    float angle = atan((rotation * BASE_WHEEL)/ speed);
    // steer = clamp99_99(steer);

    this -> steer = angle;
}

float MotorUart::getSteer(){
    return steer;
    // return 0.0;
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

    // char speedSign = speed>0 ? '+' : '-'; 
    // char steerSign = steer>0 ? '+' : '-'; 

    string speedStr = floatToString(getSpeed());
    string steerStr =floatToString(getSteer());
    string commandStr = "c"+speedStr +";"+steerStr;
    sendData(commandStr);
    
    // serialPort << "c" 
    //             << speedSign
    //             << fixed << setprecision(2) << getSpeed()<<";"
    //             << steerSign 
    //             << fixed << setprecision(2) << getSteer()<<'\n';


    cout << commandStr<<endl;

}

void MotorUart::publishControl(float speed, float steer){
    setSpeed(speed);
    setSteer(steer);
    publishControl();
}

void MotorUart::publishLidarSpeed(){
    string lidarStr= to_string(getLidarSpeed());
    // char command[4] ;

    // command[0] = 'l';
    // command[1] = (speed <=-10 || speed >=10) ? strSpeed[strSpeed.size()-2] : '0';
    // command[2] = strSpeed[strSpeed.size()-1];
    // command[3] = '\n';
    string commandStr = "l" + lidarStr;
    sendData(commandStr);

    // write(serial_port, command, sizeof(command));
}

void MotorUart::publishLidarSpeed(char speed){
    setLidarSpeed(speed);
    publishLidarSpeed();
}

void MotorUart::publishMode(){
    string modeStr= to_string(getMode());
    // char command[3] ;

    // command[0] = 'm';
    // command[1] = mode[mode.size()-1];
    // command[2] = '\n';
    string commandStr = "m" + modeStr;
    sendData(commandStr);

    // write(serial_port, command, sizeof(command));
} 

void MotorUart::publishMode(char mode){
    setMode(mode);
    publishMode();
} 
void MotorUart::sendData(string message) {
    message += "\n"; // Přidáme newline pro Teensy
    // cout<<"posilam command: "<< message;
    write(serialPort, message.c_str(), message.size());
}

void MotorUart::readData() {

    string receivedData;

    while (true){

        char buffer[64] = {0};
        read(serialPort, buffer, sizeof(buffer) - 1);
        receivedData = string(buffer);
        // cout<< "teensy: "<<receivedData;

        if(receivedData[0] != 'v'){
            return; // it is not odom msg
            cout<< "first letter is not v"<< endl;
        } 

        int index = receivedData.find('s', 0);
        if (index == -1){
            return;    //it is not odom msg
            cout<< "not s in text" << endl;

        }

        setMode(0);

        string speed = receivedData.substr(1,index-1);
        setSpeed(stof(speed));
        // cout << "speed: " << speed << endl;

        string steer = receivedData.substr(index+1);
        // cout << "steer: " << steer << endl;
        // cout << "steer2: " <<  stof(steer)<< endl;
        setSteer(stof(steer));

        this_thread::sleep_for(chrono::milliseconds(1));
    }
    

    // while (true) {
    //     getline(serialPort, receivedData);  // read one line
    //     // cout << "Přijato: " << receivedData << endl;
        
    // }
}


bool MotorUart::setConnection(string dev){
    // serialPort.open(dev, ios::in | ios::out);
    // if (!serialPort.is_open()) {
    //     std::cerr << "ERROR: connection not established!" << std::endl;
    //     return;
    // }
    // sleep(2);
    // RCLCPP_INFO(logger, "Connection established");
    // serialPort<< "hi"<<endl;
    // serialPort.flush();


    serialPort = open(dev.c_str(), O_RDWR | O_NOCTTY);
    if (serialPort == -1) {
        cerr << "Error opening serial port!" << endl;
        return false;
    }

    struct termios serialParams;
    tcgetattr(serialPort, &serialParams);
    cfsetispeed(&serialParams, BAUD_RATE);
    cfsetospeed(&serialParams, BAUD_RATE);
    serialParams.c_cflag = CS8 | CLOCAL | CREAD;
    tcsetattr(serialPort, TCSANOW, &serialParams);

    RCLCPP_INFO(logger, "Connection established");
    return true;








}







