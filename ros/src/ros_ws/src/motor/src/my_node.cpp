#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <cmath>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include "geometry_msgs/msg/twist.hpp"
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "tf2_msgs/msg/tf_message.hpp"
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

  //corrected twist
  double linear_x = 0.0;
  double angular_z = 0.0;
  
  //AV info 
  const double MAX_STEER = 0.785398163; //45 deg in rads
  const double MIN_STEER = -0.785398163; //-45 deg in rads
  const double MAX_SPEED = 1.5; // meters/second
  const double MIN_SPEED = -1.3; // meters/second
  const double WHEEL_BASE = 0.5; // distance between axles
  const double FREQ_UPDATE = 0.02; // every 20 ms publish odom and update position

  int index = 0;

  public:
    Motor()
    : Node("motor"), motor(this ->get_logger() )
    {
      pubOdom = this->create_publisher<nav_msgs::msg::Odometry>("motor/odom", 10);
      pubTfOdom = this->create_publisher<tf2_msgs::msg::TFMessage>("tf", 1);
      timer_ = this->create_wall_timer(20ms, bind(&Motor::publish_odom, this));
      subTwist = this->create_subscription<geometry_msgs::msg::Twist>("cmd_vel", 1,bind(&Motor::cmdCb, this, placeholders::_1));

    }

  private:
    MotorUart motor;
    void publish_odom()
    {

      //for manual control
      if (motor.getMode() == 0){
        linear_x = motor.getSpeed();
        angular_z = motor.getSteer(); 
      }

      //update odom pose 
      update_position();

      // header
      auto message = nav_msgs::msg::Odometry();
      message.header.stamp = this->get_clock()->now();
      message.header.frame_id = "odom";
      
      message.child_frame_id = "base_footprint";
      
      // position
      message.pose.pose.position.x = x;
      message.pose.pose.position.y = y;
      message.pose.pose.position.z = 0.0;
      
      //orientation
      tuple<double, double, double, double> rotation = yawToQuaternion(yaw);
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
      pubOdom->publish(message);


      // RCLCPP_INFO(this ->get_logger(), "pred tf2");
      
      auto messageTF = tf2_msgs::msg::TFMessage();

      auto transform_stamped = geometry_msgs::msg::TransformStamped();
      
      // header
      transform_stamped.header.stamp = this->get_clock()->now();
      transform_stamped.header.frame_id = "odom";
      transform_stamped.child_frame_id = "base_footprint";

      
      //translation
      transform_stamped.transform.translation.x = x;
      transform_stamped.transform.translation.y = y;
      transform_stamped.transform.translation.z = 0.0;
      
      //rotation
      transform_stamped.transform.rotation.x = get<0>(rotation);
      transform_stamped.transform.rotation.y = get<1>(rotation);
      transform_stamped.transform.rotation.z = get<2>(rotation);
      transform_stamped.transform.rotation.w = get<3>(rotation);

      messageTF.transforms.push_back(transform_stamped);

      // messageTF.transforms[0].header.stamp = this->get_clock()->now(); 
      // messageTF.transforms[0].header.frame_id = "odom"; 

      // messageTF.transforms[0].child_frame_id = "base_footprint";
      
      // //translation
      // messageTF.transforms[0].transform.translation.x = x;
      // messageTF.transforms[0].transform.translation.y = y;
      // messageTF.transforms[0].transform.translation.z = 0.0;
      
      // //rotation
      // messageTF.transforms[0].transform.rotation.x = get<0>(rotation);
      // messageTF.transforms[0].transform.rotation.y = get<1>(rotation);
      // messageTF.transforms[0].transform.rotation.z = get<2>(rotation);
      // messageTF.transforms[0].transform.rotation.w = get<3>(rotation);
      // RCLCPP_INFO(this ->get_logger(), "po tf2_1");

      pubTfOdom->publish(messageTF);
      // RCLCPP_INFO(this ->get_logger(), "po tf2_2");



    }



    void cmdCb(const geometry_msgs::msg::Twist::SharedPtr msg) {
      // cout<<"cmdCb "<<endl;
      // TODO
      motor.publishMode(1);

      if(motor.getMode() == 0) return; // in manual mode, you dont change speed and steer 
      
      double speed = msg -> linear.x;
      double rotation = msg -> angular.z;
      
      tie(speed, rotation) = checkCommands(speed, rotation);

      linear_x = speed;
      angular_z = rotation;
      // cout<<"control: "<<linear_x <<" "<<angular_z<<endl;
      motor.publishControl(linear_x, angular_z);
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
      double maxRotation = (abs(speed) * tan(MAX_STEER)/WHEEL_BASE);
      double minRotation = (abs(speed) * tan(MIN_STEER)/WHEEL_BASE);
      // cout << "rotation "<<maxRotation<<" "<<minRotation<<" "<<rotation<<'\n';
      // correction of steering
      if (rotation > maxRotation){
        rotation = maxRotation;
        // cout << "clamp maximum steer\n";
      }
      else if (rotation < minRotation){
        rotation = minRotation;
        // cout << "clamp minimum steer\n";
      }

      return {speed, rotation};
    }



    void update_position(){
      // cout<< "pred" << "x: "<<x<<" y: "<<y <<" yaw: "<<yaw<<endl;
      // cout<< "lin_x: "<<linear_x<<" ang_z: "<<angular_z <<endl;
      x = x + linear_x * cos(yaw) * FREQ_UPDATE;  
      y = y + linear_x * sin(yaw) * FREQ_UPDATE;
      yaw = yaw + angular_z * FREQ_UPDATE;
      yaw = clampPI_PI(yaw);  
      // cout << "po "<< "x "<<x<<" y: "<<y <<" yaw: "<<yaw<<endl;

    }
    

    rclcpp::TimerBase::SharedPtr timer_;
    rclcpp::Publisher<nav_msgs::msg::Odometry>::SharedPtr pubOdom;
    rclcpp::Publisher<tf2_msgs::msg::TFMessage>::SharedPtr pubTfOdom;
    rclcpp::Subscription<geometry_msgs::msg::Twist>::SharedPtr subTwist;

};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  rclcpp::spin(make_shared<Motor>());
  rclcpp::shutdown();
  return 0;
}