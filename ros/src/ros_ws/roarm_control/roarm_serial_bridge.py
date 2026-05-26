#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import serial

class RoArmSerialBridge(Node):
    def __init__(self):
        super().__init__('roarm_serial_bridge')
        
        # ROS2 Topic Interfaces
        self.tx_sub = self.create_subscription(String, '/serial_ctrl/tx', self.tx_callback, 10)
        self.rx_pub = self.create_publisher(String, '/serial_ctrl/rx', 10)
        
        # Hardware Initialization
        self.serial_port = '/dev/ttyUSB0'
        self.baud_rate = 115200
        
        # Note: Short timeout ensures the read_timer doesn't block the ROS loop
        try:
            self.ser = serial.Serial(self.serial_port, self.baud_rate, timeout=0.01)
            self.get_logger().info(f"VHC Bridge Connected to {self.serial_port}")
        except Exception as e:
            self.get_logger().error(f"Failed to open {self.serial_port}: {e}")
            self.ser = None

        # Buffer to accumulate incoming serial telemetry data and prevent line fragmentation
        self.rx_buffer = ""

        # Timer to poll the serial port for incoming telemetry (100Hz)
        self.read_timer = self.create_timer(0.01, self.poll_serial_rx)

    def tx_callback(self, msg):
        """Catches JSON strings from the MEC and flushes them to the ESP32."""
        if self.ser and self.ser.is_open:
            try:
                # Ensure the message ends with a newline so the ESP32 parses it immediately
                command = msg.data
                if not command.endswith('\n'):
                    command += '\n'
                self.ser.write(command.encode('utf-8'))
            except Exception as e:
                self.get_logger().error(f"Serial write error: {e}")

    def poll_serial_rx(self):
        """Continuously checks the hardware buffer and publishes to the network."""
        if self.ser and self.ser.is_open:
            try:
                # Check if data is waiting in the hardware buffer to prevent blocking
                if self.ser.in_waiting > 0:
                    data = self.ser.read(self.ser.in_waiting).decode('utf-8', errors='ignore')
                    self.rx_buffer += data
                    while '\n' in self.rx_buffer:
                        line, self.rx_buffer = self.rx_buffer.split('\n', 1)
                        line = line.strip()
                        if line:
                            msg = String()
                            msg.data = line
                            self.rx_pub.publish(msg)
            except Exception as e:
                self.get_logger().warning(f"Serial read error: {e}")

def main(args=None):
    rclpy.init(args=args)
    node = RoArmSerialBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        if node.ser and node.ser.is_open:
            node.ser.close()
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()