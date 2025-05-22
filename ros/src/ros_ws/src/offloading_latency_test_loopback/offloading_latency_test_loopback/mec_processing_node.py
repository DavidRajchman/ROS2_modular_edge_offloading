import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import time

class MECProcessingNode(Node):
    def __init__(self):
        super().__init__('mec_processing_node')
        self.declare_parameter('subscribe_topic', 'test_input_topic')
        self.declare_parameter('publish_topic', 'test_result_topic')

        self.subscribe_topic_name = self.get_parameter('subscribe_topic').get_parameter_value().string_value
        self.publish_topic_name = self.get_parameter('publish_topic').get_parameter_value().string_value
        
        self.subscription = self.create_subscription(
            String,
            self.subscribe_topic_name,
            self.listener_callback,
            10)
        self.publisher_ = self.create_publisher(String, self.publish_topic_name, 10)
        self.get_logger().info(f"MEC Processing Node started. Subscribing to '{self.subscribe_topic_name}', "
                               f"publishing to '{self.publish_topic_name}'.")

    def get_current_timestamp_ns(self):
        now_rclpy_time = self.get_clock().now()
        return now_rclpy_time.nanoseconds

    def listener_callback(self, msg):
        self.get_logger().info(f'MEC: Received: "{msg.data}"')
        
        mec_process_time_ns = self.get_current_timestamp_ns()
        
        processed_msg = String()
        processed_msg.data = f"{msg.data}_processedAtTime[{mec_process_time_ns}]"
        
        self.publisher_.publish(processed_msg)
        self.get_logger().info(f'MEC: Published: "{processed_msg.data}"')

def main(args=None):
    rclpy.init(args=args)
    mec_node = MECProcessingNode()
    try:
        rclpy.spin(mec_node)
    except KeyboardInterrupt:
        pass
    finally:
        mec_node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
