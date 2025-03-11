#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <cmath>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "../../../lib/topicClasses/MotorUart.hpp"
#include "../../../../lib/math/ROS2_math.hpp"



using namespace std::chrono_literals;
using namespace std;


class Motor : public rclcpp::Node
{
  // odom pose of AV
  double x = 0.0;
  double y = 0.0;
  double yaw = 0.0;

  // variables for messages
  int seq = 0;
  
  //corrected twist
  double linear_x = 0.0;
  double angular_z = 0.0;
  
  //AV info 
  const double MAX_STEER = 0.785398163; //45 deg in rads
  const double MIN_STEER = -0.785398163; //-45 deg in rads
  const double MAX_SPEED = 1.5; // meters/second
  const double MIN_SPEED = -1.3; // meters/second
  const double WHEEL_BASE = 0.5; // distance between axles
  const double FREQ_UPDATE = 0.02 // every 20 ms publish odom and update position

  public:
    Motor()
    : Node("motor"), count_(0), motor(this ->get_logger() )
    {
      publisher_ = this->create_publisher<std_msgs::msg::String>("motor/odom", 2);
      timer_ = this->create_wall_timer(20ms, bind(&Motor::publish_odom, this));
      subTwist = this->create_subscription<geometry_msgs::msg::Twist>("cmd_vel", 1,bind(&Motor::cmdCb, this, placeholders::_1));

    }

  private:
    MotorUart motor;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr subControl;
    void publish_odom()
    {
      //update odom pose 
      update_position();

      // header
      auto message = nav_msgs::msg::Odometry();
      message.header.seq = seq++;
      message.header.stamp = node->get_clock()->now();
      message.header.frame_id = "odom";
      
      // position
      message.pose.pose.position.x = x;
      message.pose.pose.position.y = y;
      message.pose.pose.position.z = 0.0;

      //orientation
      tuple<double, double, double, double> rotation = yawToQuaternion(yaw)
      message.pose.pose.orientation.x = get<0>(rotation);
      message.pose.pose.orientation.y = get<1>(rotation);
      message.pose.pose.orientation.z = get<2>(rotation);
      message.pose.pose.orientation.w = get<3>(rotation);

      // twist linear
      message.twist.twist.linear.x = linear_x;
      message.twist.twist.linear.y = 0.0;
      message.twist.twist.linear.z = 0.0;

      // twist angular
      message.twist.twist.angular.x = 0.0;
      message.twist.twist.angular.y = 0.0;
      message.twist.twist.angular.z = angular_z;

      //publishing
      publisher_->publish(message);
    }



    void cmdCb(const geometry_msgs::msg::Twist::SharedPtr msg) {
      double speed = msg.linear.x;
      double rotation = msg.angular.z;
      
      tie(speed, rotation) = checkCommands(double speed, double rotation)

      linear_x = speed;
      angular_z = rotation;
    }



    tuple<float, float> checkCommands(double speed, double rotation){
      //no moves
      if (speed < 0.05 && speed >-0.05)
        return {0,0};
      
      // correction fo speed
      if (speed > MAX_SPEED)
        speed = MAX_SPEED;
      else if (speed < MIN_SPEED)
        speed = MIN_SPEED;

      // calculate max and min rotation
      double maxRotation = (speed * tan(MAX_STEER)/WHEEL_BASE);
      double minRotation = (speed * tan(MIN_STEER)/WHEEL_BASE);
        
      // correction of steering
      if (rotation > maxRotation)
        rotation = maxRotation;
      else if (rotation < minRotation)
        rotation = minRotation;

      return {speed, rotation};
    }



    void update_position(){
      x = x + linear_x * cos(yaw) * FREQ_UPDATE;  
      y = y + linear_x * sin(yaw) * FREQ_UPDATE;
      yaw = yaw + angular_z * FREQ_UPDATE;
      yaw = clampPI_PI(yaw);  
    }
    

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<std_msgs::msg::String>::SharedPtr publisher_;
    size_t count_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(make_shared<Motor>());
  rclcpp::shutdown();
  return 0;
}