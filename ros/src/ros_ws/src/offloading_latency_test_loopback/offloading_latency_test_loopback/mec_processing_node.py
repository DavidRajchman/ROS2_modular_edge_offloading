import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import time

class MECProcessingNode(Node):
    def __init__(self):
        super().__init__('mec_processing_node')
        self.declare_parameter('subscribe_topic', 'test_input_topic')
        self.declare_parameter('publish_topic', 'test_result_topic')
        self.declare_parameter('fixed_processing_delay_ms', 100.0) 

        self.subscribe_topic_name = self.get_parameter('subscribe_topic').get_parameter_value().string_value
        self.publish_topic_name = self.get_parameter('publish_topic').get_parameter_value().string_value
        self.fixed_processing_delay_ns = int(self.get_parameter('fixed_processing_delay_ms').get_parameter_value().double_value * 1_000_000)
        
        self.subscription = self.create_subscription(
            String,
            self.subscribe_topic_name,
            self.listener_callback,
            10) # QoS depth
        self.publisher_ = self.create_publisher(String, self.publish_topic_name, 10) # QoS depth
        self.get_logger().info(f"MEC Processing Node started. Subscribing to '{self.subscribe_topic_name}', "
                               f"publishing to '{self.publish_topic_name}'. "
                               f"Fixed processing delay: {self.fixed_processing_delay_ns / 1_000_000.0} ms.")

    def get_current_timestamp_ns(self):
        # Using ROS clock for consistency with sim time if used
        return self.get_clock().now().nanoseconds

    def listener_callback(self, msg):
        mec_receive_time_ns = self.get_current_timestamp_ns()
        self.get_logger().debug(f'MEC: Received: "{msg.data}" at {mec_receive_time_ns}')

        # --- Prepare the message content BEFORE the delay ---
        # The timestamp embedded in the message will be the target publish time.
        # This represents the completion of the (receive_time + fixed_delay) window.
        intended_publish_time_ns = mec_receive_time_ns + self.fixed_processing_delay_ns
        
        processed_msg = String()
        processed_msg.data = f"{msg.data}_processedAtTime[{intended_publish_time_ns}]"
        # --- Message preparation is done ---

        # --- Wait until the current time is at or after the intended publish time ---
        # This loop ensures we wait for the fixed delay.
        current_time_ns = self.get_current_timestamp_ns()
        while current_time_ns < intended_publish_time_ns:
            # Sleep for a very short duration to yield CPU and approach the target time.
            # A smaller sleep yields more precision but higher CPU usage during the wait.
            # For a 100ms delay, 100us (0.0001s) or 1ms (0.001s) sleep is reasonable.
            # If time.sleep precision is an issue, a more active spin might be needed,
            # but time.sleep is generally preferred for not hogging CPU.
            sleep_duration_s = max(0, (intended_publish_time_ns - current_time_ns) / 1_000_000_000.0 * 0.5) # Sleep for half the remaining time, or a small fixed amount
            time.sleep(min(sleep_duration_s, 0.0001)) # Cap sleep to avoid oversleeping if remaining time is very small
            current_time_ns = self.get_current_timestamp_ns()
        # --- Wait is complete ---

        # --- Publish immediately ---
        self.publisher_.publish(processed_msg)
        actual_publish_time_ns = self.get_current_timestamp_ns() # Capture actual time immediately after publish call returns

        self.get_logger().info(f'MEC: Published: "{processed_msg.data}" at {actual_publish_time_ns} (Intended: {intended_publish_time_ns}, Received: {mec_receive_time_ns})')
        # Log deviation if significant
        deviation_ms = (actual_publish_time_ns - intended_publish_time_ns) / 1_000_000.0
        if abs(deviation_ms) > 1.0 : # Log if deviation is more than 1ms
             self.get_logger().warn(f'MEC: Publish time deviation from intended: {deviation_ms:.3f} ms')


def main(args=None):
    rclpy.init(args=args)
    mec_node = MECProcessingNode()
    try:
        rclpy.spin(mec_node)
    except KeyboardInterrupt:
        pass
    finally:
        # Cleanup
        mec_node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()