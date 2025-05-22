import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import time

class VHCNode(Node):
    def __init__(self):
        super().__init__('vhc_node')
        self.declare_parameter('publish_topic', 'test_input_topic')
        self.declare_parameter('subscribe_topic', 'test_result_topic')
        self.declare_parameter('publish_interval_sec', 1.0)
        self.declare_parameter('message_counter_max', 9999) # Max value for the counter part

        self.publish_topic_name = self.get_parameter('publish_topic').get_parameter_value().string_value
        self.subscribe_topic_name = self.get_parameter('subscribe_topic').get_parameter_value().string_value
        publish_interval = self.get_parameter('publish_interval_sec').get_parameter_value().double_value
        self.message_counter_max_val = self.get_parameter('message_counter_max').get_parameter_value().integer_value


        self.publisher_ = self.create_publisher(String, self.publish_topic_name, 10)
        self.subscription = self.create_subscription(
            String,
            self.subscribe_topic_name,
            self.result_listener_callback,
            10)
        
        self.timer = self.create_timer(publish_interval, self.timer_callback)
        self.message_counter = 0 # Start counter at 0, will become 1 on first message
        self.get_logger().info(f"VHC Node started. Publishing to '{self.publish_topic_name}', "
                               f"subscribing to '{self.subscribe_topic_name}'.")

    def get_current_timestamp_ns(self):
        now_rclpy_time = self.get_clock().now()
        return now_rclpy_time.nanoseconds

    def timer_callback(self):
        # Increment counter
        self.message_counter += 1
        # Check for overflow and reset if necessary
        if self.message_counter > self.message_counter_max_val:
            self.message_counter = 1 # Reset to 1 after overflow

        msg = String()
        vhc_send_time_ns = self.get_current_timestamp_ns()
        # Format counter with leading zeros, e.g., 0001, 0010, 0100, 1000
        # The number of zeros in the format string (e.g., :04d) determines the padding
        # Adjust if self.message_counter_max_val has a different number of digits
        counter_str_length = len(str(self.message_counter_max_val))
        msg.data = f'testmsg_{self.message_counter:0{counter_str_length}d}_[{vhc_send_time_ns}]'
        
        self.publisher_.publish(msg)
        self.get_logger().info(f'VHC: Published: "{msg.data}"')

    def result_listener_callback(self, msg):
        vhc_receive_time_ns = self.get_current_timestamp_ns()
        received_data = msg.data
        self.get_logger().info(f'VHC: Received: "{received_data}"')

        try:
            parts = received_data.split('_processedAtTime')
            if len(parts) < 2:
                self.get_logger().error(f"VHC: Could not parse MEC timestamp from: {received_data}")
                return

            main_part = parts[0] 
            mec_time_str = parts[1].strip('[]')

            original_msg_parts = main_part.split('_[')
            if len(original_msg_parts) < 2:
                self.get_logger().error(f"VHC: Could not parse original VHC timestamp from: {main_part}")
                return
            
            message_id_part = original_msg_parts[0] # e.g., testmsg_0001
            vhc_send_time_str = original_msg_parts[1].strip('[]')

            vhc_send_time_ns_orig = int(vhc_send_time_str)
            mec_process_time_ns = int(mec_time_str)

            vhc_to_mec_delay_ms = (mec_process_time_ns - vhc_send_time_ns_orig) / 1_000_000.0
            mec_to_vhc_delay_ms = (vhc_receive_time_ns - mec_process_time_ns) / 1_000_000.0
            total_end_to_end_delay_ms = (vhc_receive_time_ns - vhc_send_time_ns_orig) / 1_000_000.0
            
            self.get_logger().info(
                f"--- Latency Report for {message_id_part} ---\n"
                f"  VHC Send Time (ns):         {vhc_send_time_ns_orig}\n"
                f"  MEC Process Time (ns):      {mec_process_time_ns}\n"
                f"  VHC Receive Time (ns):      {vhc_receive_time_ns}\n"
                f"  ---------------------------------------\n"
                f"  VHC Send -> MEC Process:    {vhc_to_mec_delay_ms:.3f} ms\n"
                f"  MEC Process -> VHC Receive: {mec_to_vhc_delay_ms:.3f} ms\n"
                f"  Total End-to-End Delay:     {total_end_to_end_delay_ms:.3f} ms\n"
                f"---------------------------------------"
            )

        except ValueError as e:
            self.get_logger().error(f"VHC: Error parsing timestamps: {e}. Data: {received_data}")
        except Exception as e:
            self.get_logger().error(f"VHC: An unexpected error occurred: {e}. Data: {received_data}")

def main(args=None):
    rclpy.init(args=args)
    vhc_node = VHCNode()
    try:
        rclpy.spin(vhc_node)
    except KeyboardInterrupt:
        pass
    finally:
        vhc_node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
