import rclpy
from rclpy.node import Node
import threading
import json
import serial

from sensor_msgs.msg import JointState
from std_srvs.srv import SetBool

class RoArmHardwareBridge(Node):

    def __init__(self):
        super().__init__('roarm_hardware_bridge')
        
        # 1. Serial Setup (Added timeout for non-blocking reads)
        try:
            self.ser = serial.Serial("/dev/ttyUSB0", 115200, timeout=0.05)
            self.get_logger().info("Connected to RoArm-M1 on /dev/ttyUSB0")
        except serial.SerialException as e:
            self.get_logger().error(f"Failed to connect to serial port: {e}")
            raise SystemExit

        # 2. Subscriptions & Publishers
        self.subscription = self.create_subscription(
            JointState,
            'joint_states', # Listen to the MEC commands
            self.cmd_callback,
            10)
            
        # --- TELEMETRY COMMENTED OUT ---
        # self.actual_pub = self.create_publisher(
        #     JointState, 
        #     'joint_states_actual', # Publish real hardware state back to the MEC
        #     10)

        # 3. Emergency Torque Service
        self.torque_srv = self.create_service(
            SetBool, 
            'set_torque', 
            self.torque_callback)

        # 4. Background Telemetry Thread
        self.running = True
        
        # --- TELEMETRY COMMENTED OUT ---
        # self.read_thread = threading.Thread(target=self.serial_read_loop, daemon=True)
        # self.read_thread.start()

    def posGet(self, radInput, direcInput, multiInput):
        if radInput == 0:
            return 2047
        else:
            # Added a clamp to absolutely ensure we never send an out-of-bounds tick
            getPos = int(2047 + (direcInput * radInput / 3.1415926 * 2048 * multiInput) + 0.5)
            return max(0, min(4095, getPos))

    def cmd_callback(self, msg):
        pos = msg.position
        
        # Fallback to 0 (max speed) if the MEC doesn't provide velocity data
        vel = msg.velocity if len(msg.velocity) >= 5 else [0, 0, 0, 0, 0]
        
        join1 = self.posGet(pos[0], -1, 1)
        join2 = self.posGet(pos[1], -1, 3)
        join3 = self.posGet(pos[2], -1, 1)
        join4 = self.posGet(pos[3],  1, 1)
        join5 = self.posGet(pos[4], -1, 1)
        
        # T:3 Positional Command
        payload = {
            'T': 3,
            'P1': join1, 'P2': join2, 'P3': join3, 'P4': join4, 'P5': join5,
            'S1': int(vel[0]), 'S2': int(vel[1]), 'S3': int(vel[2]), 'S4': int(vel[3]), 'S5': int(vel[4]),
            'A1': 60, 'A2': 60, 'A3': 60, 'A4': 60, 'A5': 60
        }
        
        try:
            data = json.dumps(payload) + "\n"
            self.ser.write(data.encode('utf-8'))
        except Exception as e:
            self.get_logger().error(f"Serial write error: {e}")

    def torque_callback(self, request, response):
        """ Instantly cuts or enables servo power """
        command = 1 if request.data else 0
        payload = {"T": 8, "P1": command}
        
        try:
            self.ser.write((json.dumps(payload) + "\n").encode('utf-8'))
            response.success = True
            response.message = f"Torque set to {request.data}"
        except Exception as e:
            response.success = False
            response.message = str(e)
            
        return response

    # --- TELEMETRY COMMENTED OUT ---
    # def serial_read_loop(self):
    #     """ Constantly monitors the ESP32 for feedback and streams it to ROS 2 """
    #     while self.running and rclpy.ok():
    #         if self.ser.in_waiting:
    #             try:
    #                 line = self.ser.readline().decode('utf-8').strip()
    #                 if not line:
    #                     continue
    #                     
    #                 # Filter for valid JSON responses
    #                 if line.startswith('{') and line.endswith('}'):
    #                     resp = json.loads(line)
    #                     
    #                     # If it's a T:3 acknowledgment, we can parse the exact positions
    #                     if resp.get("T") == 3:
    #                         msg = JointState()
    #                         msg.header.stamp = self.get_clock().now().to_msg()
    #                         msg.name = ['joint1', 'joint2', 'joint3', 'joint4', 'joint5']
    #                         
    #                         # Convert 12-bit ticks back to radians for the MEC
    #                         # (You will need to invert your posGet logic here)
    #                         msg.position = [
    #                             float(resp.get("P1", 2047)), 
    #                             float(resp.get("P2", 2047)), 
    #                             float(resp.get("P3", 2047)), 
    #                             float(resp.get("P4", 2047)), 
    #                             float(resp.get("P5", 2047))
    #                         ]
    #                         self.actual_pub.publish(msg)
    #                         
    #             except json.JSONDecodeError:
    #                 pass # Ignore debug print statements from the ESP32
    #             except Exception as e:
    #                 self.get_logger().error(f"Read loop error: {e}")

    def destroy_node(self):
        self.running = False
        
        # --- TELEMETRY COMMENTED OUT ---
        # self.read_thread.join(timeout=1.0)
        
        if self.ser.is_open:
            self.ser.close()
        super().destroy_node()

def main(args=None):
    rclpy.init(args=args)
    bridge_node = RoArmHardwareBridge()
    
    try:
        rclpy.spin(bridge_node)
    except KeyboardInterrupt:
        pass
    finally:
        bridge_node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()