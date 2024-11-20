#include "PursePursuitClass.hpp"

using namespace std;

PurePursuit::PurePursuit() : Node("pure_pusuit"), control(false)
{
    RCLCPP_INFO(this->get_logger(), "Initializing PurePursuit node...");

    // parameters
    DEBUG = this->declare_parameter<bool>("DEBUG", false);
    MAX_DIST = this->declare_parameter<double>("MAX_DISTANCE_DELETE_POINT", 0.5);
    // widthCar = node_->declare_parameter<double>("CAR_WIDTH_FOR_LIDAR", 0.8);
    LOOK_DISTANCE = this->declare_parameter<double>("LOOK_DISTANCE", 1.0);
    KP_LOW = this->declare_parameter<double>("KP_LOW", 1.5);
    KP_HIGH = this->declare_parameter<double>("KP_HIGH", 5.0);
    // LSF = node_->declare_parameter<double>("LENGHT_CENTER_FORWARD", 0.114478);
    // LSB = node_->declare_parameter<double>("LENGHT_CENTER_BACKWARD", 0.353056);
    // indexFL = node_->declare_parameter<int>("LIDAR_INDEX_FORWARD_LEFT", 630);
    // indexFR = node_->declare_parameter<int>("LIDAR_INDEX_FORWARD_RIGHT", 535);
    // indexBL = node_->declare_parameter<int>("LIDAR_INDEX_BACKWARD_LEFT", 1050);
    // indexBR = node_->declare_parameter<int>("LIDAR_INDEX_BACKWARD_RIGHT", 90);
    // rangeF = node_->declare_parameter<double>("RANGE_LIDAR_FORWARD", 0.425);
    // rangeB = node_->declare_parameter<double>("RANGE_LIDAR_BACKWARD", 0.22);
    // radiusF = node_->declare_parameter<double>("RADIUS_LIDAR_FORWARD", 0.65);
    // radiusB = node_->declare_parameter<double>("RADIUS_LIDAR_BACKWARD", 0.75);
    // angleF = node_->declare_parameter<double>("ANGLE_OF_CENTER_OF_CIRCLE_FORWARD", 1.745329);
    // angleB = node_->declare_parameter<double>("ANGLE_OF_CENTER_OF_CIRCLE_BACKWARD", 1.22173);

    motorMsg = {"manual", 0, 0};

    // subscribers and publishers
    motorPub = this->create_publisher<services::msg::ControlMotor>("control_motor", 1);
    poseSub = this->create_subscription<services::msg::Position>("robot_position", 1, std::bind(&PurePursuit::poseCb, this, std::placeholders::_1));
    pathSub = this->create_subscription<nav_msgs::msg::Path>("path", 1, std::bind(&PurePursuit::pathCb, this, std::placeholders::_1));
}

void PurePursuit::poseCb(const services::msg::Position::SharedPtr msg)
{
    // current robot position
    poseRobX = msg->pose_rob_x;
    poseRobY = msg->pose_rob_y;
    poseRobPhi = msg->pose_rob_phi;

    // when following planned path
    if (control)
    {
        followPath();
    }
}

void PurePursuit::pathCb(const nav_msgs::msg::Path::SharedPtr msg)
{

    // Inicializace proměnných
    // obstacle = false;
    // obstacleFront = false;
    // direction = 0;
    // poseXDiff = poseRobX;
    // poseYDiff = poseRobY;

    path.resize(msg->poses.size());
    for (int i = 0; i < static_cast<int>(msg->poses.size()); i++)
    {
        path[i].first = msg->poses[i].pose.position.x;
        path[i].second = msg->poses[i].pose.position.y;
    }

    // Příprava cesty pro výpočet checkpointů
    indexFollow = 0;
    pathFollow.clear();
    copy(path.begin(), path.end(), back_inserter(pathFollow));

    followPath();

    publishMode("auto");
}

void PurePursuit::publishMode(string mode)
{
    motorMsg.mode = mode;

    if (mode == "auto")
        control = true;
    else if (mode == "manual")
    {
        control = false;
        motorMsg.speed = 0;
        motorMsg.steer = 0;
    }

    services::msg::ControlMotor motor_msg;

    motor_msg.mode = motorMsg.mode;
    motor_msg.forwarding = motorMsg.speed;
    motor_msg.steering = motorMsg.steer;

    motorPub->publish(motor_msg);
}

void PurePursuit::controlMotor(int speed, int steer)
{
    if (motorMsg.mode == "manual" || control == false)
        return;

    motorMsg.speed = speed;
    motorMsg.steer = steer;

    services::msg::ControlMotor motor_msg;

    motor_msg.mode = "";
    motor_msg.forwarding = motorMsg.speed;
    motor_msg.steering = motorMsg.steer;

    motorPub->publish(motor_msg);
}

// // Publikování příkazu pro automatický režim

void PurePursuit::followPath()
{
    int i = 0;
    // float LOOK_DISTANCE = lookDist;
    float distance = 0;
    int pathSize = static_cast<int>(pathFollow.size());

    while (distance < MAX_DIST && !path.empty())
    {
        distance = sqrt(pow(poseRobX - path[0].first, 2) + pow(poseRobY - path[0].second, 2));

        // Odstranit bod z fronty
        if (distance < MAX_DIST)
        {
            path.erase(path.begin());
            // if (DEBUG) {
            //     markerArr.markers[1].action = visualization_msgs::msg::Marker::DELETE;
            //     markerArr.markers.erase(markerArr.markers.begin());
            //     markerArrPub->publish(markerArr);
            // }
        }

        double pursuitX, pursuitY;
        // Najít bod ve specifické vzdálenosti
        if (indexFollow != pathSize - 2)
        {
            int j = indexFollow;
            float distance1 = abs(sqrt(pow(poseRobX - pathFollow[j].first, 2) + pow(poseRobY - pathFollow[j].second, 2)));
            float distance2 = abs(sqrt(pow(poseRobX - pathFollow[j + 1].first, 2) + pow(poseRobY - pathFollow[j + 1].second, 2)));

            while ((!(distance2 > LOOK_DISTANCE) && j + 1 < pathSize) || (distance1 > distance2 && pathSize > j + 1))
            {
                j++;
                distance1 = abs(sqrt(pow(poseRobX - pathFollow[j].first, 2) + pow(poseRobY - pathFollow[j].second, 2)));
                distance2 = abs(sqrt(pow(poseRobX - pathFollow[j + 1].first, 2) + pow(poseRobY - pathFollow[j + 1].second, 2)));
            }

            // float lenghtLine = abs(sqrt(pow(pathFollow[j].first - pathFollow[j + 1].first, 2) + pow(pathFollow[j].second - pathFollow[j + 1].second, 2)));
            float ratio = (1 - distance1 / LOOK_DISTANCE) / (distance2 / LOOK_DISTANCE - distance1 / LOOK_DISTANCE);
            ratio = clamp(ratio, 0.0f, 1.0f);

            // Najít bod na přímce mezi vnějšími body
            pursuitX = (pathFollow[j + 1].first - pathFollow[j].first) * ratio + pathFollow[j].first;
            pursuitY = (pathFollow[j + 1].second - pathFollow[j].second) * ratio + pathFollow[j].second;
            indexFollow = j;

            i++;
            // if (debug) {
            //     marker.pose.position.x = pursuitX;
            //     marker.pose.position.y = pursuitY;
            //     markerPub->publish(marker);
            // }
        }
        else if (indexFollow == pathSize - 2)
        { // V případě posledního bodu
            pursuitX = pathFollow.back().first;
            pursuitY = pathFollow.back().second;

            i++;
            // if (debug) {
            //     marker.pose.position.x = pursuitX;
            //     marker.pose.position.y = pursuitY;
            //     markerPub->publish(marker);
            // }
        }

        // Podmínka zastavení
        if (path.empty())
        {
            // poseXDiff -= poseRobX;
            // poseYDiff -= poseRobY;
            // float delka = sqrt(pow(poseXDiff, 2) + pow(poseYDiff, 2));
            RCLCPP_INFO(this->get_logger(), "Following finished.");

            // control = false;

            // stage.data = "init";
            // stagePub->publish(stage);
            publishMode("manual");
            return;
        }

        findAngle(KP_HIGH, pursuitX, pursuitY);
    }
}


void PurePursuit::findAngle(float Kp, double pursuitX, double pursuitY) {
    float difX, difY, angleMap;
    // float distpoint;
    int angle;

    // Výpočet rozdílů v souřadnicích
    difX = pursuitX - poseRobX;
    difY = pursuitY - poseRobY;

    // Vzdálenost k bodu a úhel na mapě
    // distPoint = std::sqrt(difX * difX + difY * difY);
    angleMap = std::atan2(difY, difX);

    // Rozdíl úhlů
    double difAngle = poseRobPhi - angleMap;

    // Převod do rozsahu -PI až PI
    while (difAngle <= -M_PI || difAngle >= M_PI) {
        if (difAngle <= -M_PI) {
            difAngle += 2 * M_PI;
        } else {
            difAngle -= 2 * M_PI;
        }
    }

    // PI regulátor
    sumAngle -= difAngle;
    if (control) {
        angle = std::round(Kp * (-difAngle * 180 / M_PI) + sumAngle * 0.2); // 0.35

        // Anti-windup ochrana
        if (sumAngle > 60) {
            sumAngle = 60;
        } else if (sumAngle < -60) {
            sumAngle = -60;
        }

        // Publikování rychlosti a úhlu
        controlMotor(18, angle);
    } else {
        sumAngle = 0;
    }
}
