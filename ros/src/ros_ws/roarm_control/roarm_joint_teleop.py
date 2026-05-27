#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
import tkinter as tk
import threading
import math
import time

class RoArmJointTeleop(Node):
    def __init__(self):
        super().__init__('roarm_joint_teleop')
        self.publisher_ = self.create_publisher(JointState, 'joint_states', 10)
        self.get_logger().info("Direct Joint Teleop Node running. Publishing to /joint_states.")

        self.pressed_keys = {}

        # The commanded joint states (radians)
        self.target_joints = [0.0, 0.0, 0.0, 0.0, 0.0]
        self.cmd_joints = [0.0, 0.0, 0.0, 0.0, 0.0]

        # Velocity Limits
        self.operator_speed = 1.0       # rad/s for main joints
        self.gripper_speed = 0.5        # rad/s for gripper
        self.arm_speed = 30.0           # Fake velocity for serial_ctrl_py 

        # Joint Limits (approximate physical limits in radians)
        self.joint_limits = [
            (-math.pi, math.pi),     # Base
            (-math.pi/2, math.pi/2), # Shoulder
            (-math.pi, math.pi),     # Elbow
            (-math.pi, math.pi),     # Wrist
            (-math.pi, math.pi)      # Gripper
        ]

        self.hz = 30.0
        self.last_time = time.time()
        self.timer = self.create_timer(1.0 / self.hz, self.control_loop)

    def control_loop(self):
        current_time = time.time()
        dt = current_time - self.last_time
        self.last_time = current_time

        d_joints = [0.0, 0.0, 0.0, 0.0, 0.0]

        if self.pressed_keys.get('q'): d_joints[0] += 1
        if self.pressed_keys.get('e'): d_joints[0] -= 1
        
        if self.pressed_keys.get('w'): d_joints[1] += 1
        if self.pressed_keys.get('s'): d_joints[1] -= 1
        
        if self.pressed_keys.get('a'): d_joints[2] += 1
        if self.pressed_keys.get('d'): d_joints[2] -= 1
        
        if self.pressed_keys.get('up'):    d_joints[3] += 1
        if self.pressed_keys.get('down'):  d_joints[3] -= 1
        
        if self.pressed_keys.get('left'):  d_joints[4] += 1
        if self.pressed_keys.get('right'): d_joints[4] -= 1

        for i in range(5):
            speed = self.operator_speed if i < 4 else self.gripper_speed
            self.target_joints[i] += d_joints[i] * speed * dt
            
            # Clamp to limits
            min_val, max_val = self.joint_limits[i]
            self.target_joints[i] = max(min_val, min(max_val, self.target_joints[i]))

            # Chase target (smooth movement)
            diff = self.target_joints[i] - self.cmd_joints[i]
            step = speed * dt
            if abs(diff) > step:
                self.cmd_joints[i] += math.copysign(step, diff)
            else:
                self.cmd_joints[i] = self.target_joints[i]

        msg = JointState()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.name = ['base', 'shoulder', 'elbow', 'pitch', 'gripper']
        msg.position = self.cmd_joints
        msg.velocity = [self.arm_speed] * 5

        self.publisher_.publish(msg)

def run_tkinter_gui(node):
    root = tk.Tk()
    root.title("RoArm-M1 Direct Joint Teleop")
    root.geometry("350x160")

    text = ("Mode: DIRECT JOINT CONTROL\n\n"
            "Q/R: Base Rotation\n"
            "W/S: Shoulder\n"
            "A/D: Elbow\n"
            "Up/Down: Wrist\n"
            "Left/Right: Gripper")
            
    label = tk.Label(root, text=text, font=("Helvetica", 11))
    label.pack(pady=20)

    def key_press(event): 
        node.pressed_keys[event.keysym.lower()] = True
        
    def key_release(event): 
        node.pressed_keys[event.keysym.lower()] = False

    root.bind("<KeyPress>", key_press)
    root.bind("<KeyRelease>", key_release)
    
    def on_closing():
        root.destroy()
        rclpy.shutdown()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()

def main(args=None):
    rclpy.init(args=args)
    node = RoArmJointTeleop()
    
    # Run ROS 2 spin in a background thread so Tkinter can run on the main thread
    ros_thread = threading.Thread(target=rclpy.spin, args=(node,), daemon=True)
    ros_thread.start()
    
    run_tkinter_gui(node)

if __name__ == '__main__':
    main()
