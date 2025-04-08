#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "mqtt/async_client.h"

class RosToMqttGateway : public rclcpp::Node
{
public:
  RosToMqttGateway()
  : Node("ros_mqtt_gateway")
  {
    // Initialize parameters with defaults
    declare_parameter("mqtt_server_uri", "davidpraha.buru-palermo.ts.net");
    //declare_parameter("mqtt_client_id", "ros_gateway324132adsfdfas");
    declare_parameter("mqtt_topic", "test_rosgw_david");
    declare_parameter("mqtt_qos", 1);
    declare_parameter("ros_topic", "topic");
    
    // Get parameters
    mqtt_server_uri_ = get_parameter("mqtt_server_uri").as_string();
    mqtt_client_id_ = get_parameter("mqtt_client_id").as_string();
    mqtt_topic_ = get_parameter("mqtt_topic").as_string();
    mqtt_qos_ = get_parameter("mqtt_qos").as_int();
    std::string ros_topic = get_parameter("ros_topic").as_string();
    
    RCLCPP_INFO(get_logger(), "Initializing MQTT connection to %s", mqtt_server_uri_.c_str());
    
    // Setup MQTT client
    setup_mqtt_client();
    
    // Create ROS subscription
    subscription_ = create_subscription<std_msgs::msg::String>(
      ros_topic, 10,
      [this](std_msgs::msg::String::UniquePtr msg) {
        RCLCPP_INFO(this->get_logger(), "Received from ROS: '%s'", msg->data.c_str());
        publish_to_mqtt(msg->data);
      }
    );
    
    // Set up shutdown handler
    rclcpp::on_shutdown([this]() { disconnect_mqtt(); });
  }

private:
  void setup_mqtt_client() {
    try {
      // Create MQTT client
      mqtt_client_ = std::make_shared<mqtt::async_client>(mqtt_server_uri_, mqtt_client_id_);
      
      // Connect options
      mqtt::connect_options conn_opts;
      conn_opts.set_keep_alive_interval(20);
      conn_opts.set_clean_session(true);
      
      RCLCPP_INFO(get_logger(), "Connecting to MQTT broker...");
      auto token = mqtt_client_->connect(conn_opts);
      token->wait();
      
      // Create topic object for publishing
      mqtt_topic_obj_ = std::make_shared<mqtt::topic>(*mqtt_client_, mqtt_topic_, mqtt_qos_);
      
      RCLCPP_INFO(get_logger(), "Connected to MQTT broker successfully");
    }
    catch (const mqtt::exception& exc) {
      RCLCPP_ERROR(get_logger(), "MQTT connection failed: %s", exc.what());
    }
  }
  
  void publish_to_mqtt(const std::string& message) {
    if (!mqtt_client_ || !mqtt_client_->is_connected()) {
      RCLCPP_WARN(get_logger(), "MQTT client not connected, trying to reconnect...");
      setup_mqtt_client();
      if (!mqtt_client_ || !mqtt_client_->is_connected()) {
        RCLCPP_ERROR(get_logger(), "Failed to reconnect to MQTT broker");
        return;
      }
    }
    
    try {
      mqtt_topic_obj_->publish(message)->wait();
      RCLCPP_INFO(get_logger(), "Published to MQTT topic %s: %s", 
                  mqtt_topic_.c_str(), message.c_str());
    }
    catch (const mqtt::exception& exc) {
      RCLCPP_ERROR(get_logger(), "MQTT publish failed: %s", exc.what());
    }
  }
  
  void disconnect_mqtt() {
    if (mqtt_client_ && mqtt_client_->is_connected()) {
      RCLCPP_INFO(get_logger(), "Disconnecting from MQTT broker...");
      try {
        mqtt_client_->disconnect()->wait();
        RCLCPP_INFO(get_logger(), "Disconnected from MQTT broker");
      }
      catch (const mqtt::exception& exc) {
        RCLCPP_ERROR(get_logger(), "MQTT disconnect failed: %s", exc.what());
      }
    }
  }

  // ROS subscription
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr subscription_;
  
  // MQTT variables
  std::shared_ptr<mqtt::async_client> mqtt_client_;
  std::shared_ptr<mqtt::topic> mqtt_topic_obj_;
  std::string mqtt_server_uri_;
  std::string mqtt_client_id_;
  std::string mqtt_topic_;
  int mqtt_qos_;
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<RosToMqttGateway>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}