#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
import tkinter as tk
import threading
import math
import time
from enum import IntEnum

class Joint(IntEnum):
    BASE = 0
    SHOULDER = 1
    ELBOW = 2
    WRIST = 3
    GRIPPER = 4

class RoArmJointTeleop(Node):
    def __init__(self):
        super().__init__('roarm_joint_teleop')
        self.publisher_ = self.create_publisher(JointState, 'joint_states', 10)
        self.get_logger().info("Direct Joint Teleop Node running. Publishing to /joint_states.")

        self.pressed_keys = {}

        # The commanded joint states (radians)
        self.target_joints = [0.0, 0.0, 0.0, 0.0, 0.0]
        self.cmd_joints = [0.0, 0.0, 0.0, 0.0, 0.0]
        self.last_published_joints = [None, None, None, None, None]

        # Auto Servo Speed feature
        self.auto_servo_speed = True
        self.manual_servo_speed = 3000.0

        # Base Velocity (rad/s) and Custom Multipliers 
        # Keep values below 1, to prevent speed from exceeding the hard limits 
        # and avoid jerky movements, or motor stalling.
        self.base_speed = 1.0       
        self.joint_speed_multipliers = {
            Joint.BASE: 0.7,      # 30% slower
            Joint.SHOULDER: 0.9, 
            Joint.ELBOW: 1.0,
            Joint.WRIST: 1.0,
            Joint.GRIPPER: 0.5    # 50% slower
        }
        
        self.arm_speed = 30.0           # Fake velocity for serial_ctrl_py 

        # Joint Limits (Absolute Servo Tick Limits: 0 to 4095)
        # Based on serial_ctrl_py mapping: pos = 2047 + (dir * rad / pi * 2048 * multi)
        self.joint_limits = {
            Joint.BASE: (-math.pi, math.pi),           # multi = 1
            Joint.SHOULDER: (-math.pi/3.0, math.pi/3.0), # multi = 3
            Joint.ELBOW: (-math.pi, math.pi),          # multi = 1
            Joint.WRIST: (-math.pi, math.pi),          # multi = 1
            Joint.GRIPPER: (-math.pi, math.pi)         # multi = 1
        }
        
        self.joint_multis = {
            Joint.BASE: 1.0,
            Joint.SHOULDER: 3.0,
            Joint.ELBOW: 1.0,
            Joint.WRIST: 1.0,
            Joint.GRIPPER: 1.0
        }

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
        
        if self.pressed_keys.get('d'): d_joints[2] += 1
        if self.pressed_keys.get('a'): d_joints[2] -= 1
        
        if self.pressed_keys.get('up'):    d_joints[3] += 1
        if self.pressed_keys.get('down'):  d_joints[3] -= 1
        
        if self.pressed_keys.get('left'):  d_joints[4] += 1
        if self.pressed_keys.get('right'): d_joints[4] -= 1

        for i in range(5):
            joint = Joint(i)
            speed = self.base_speed * self.joint_speed_multipliers[joint]
                
            self.target_joints[i] += d_joints[i] * speed * dt
            
            # Clamp to limits
            min_val, max_val = self.joint_limits[joint]
            self.target_joints[i] = max(min_val, min(max_val, self.target_joints[i]))

            # Chase target (smooth movement)
            diff = self.target_joints[i] - self.cmd_joints[i]
            step = speed * dt
            if abs(diff) > step:
                self.cmd_joints[i] += math.copysign(step, diff)
            else:
                self.cmd_joints[i] = self.target_joints[i]

        should_publish = False
        for i in range(5):
            if self.last_published_joints[i] is None or abs(self.cmd_joints[i] - self.last_published_joints[i]) > 1e-4:
                should_publish = True
                break

        if should_publish:
            msg = JointState()
            msg.header.stamp = self.get_clock().now().to_msg()
            msg.name = ['base', 'shoulder', 'elbow', 'pitch', 'gripper']
            msg.position = self.cmd_joints
            
            # Calculate Servo Velocity (Ticks per second)
            msg.velocity = [0.0] * 5
            for i in range(5):
                if self.auto_servo_speed:
                    if self.last_published_joints[i] is not None:
                        delta_rad = abs(self.cmd_joints[i] - self.last_published_joints[i])
                        ticks_per_rad = (2048.0 / math.pi) * self.joint_multis[Joint(i)]
                        req_speed = delta_rad * ticks_per_rad * self.hz * 1.5  # 50% buffer to prevent stalling
                        msg.velocity[i] = max(10.0, min(3000.0, req_speed))
                    else:
                        msg.velocity[i] = 3000.0
                else:
                    msg.velocity[i] = self.manual_servo_speed

            self.publisher_.publish(msg)
            self.last_published_joints = self.cmd_joints.copy()

def run_tkinter_gui(node):
    root = tk.Tk()
    root.title("RoArm-M1 Direct Joint Teleop")
    root.geometry("450x700")

    text = ("Mode: DIRECT JOINT CONTROL")
            
    label = tk.Label(root, text=text, font=("Helvetica", 12, "bold"))
    label.pack(pady=10)

    # Speed Slider
    speed_frame = tk.Frame(root)
    speed_frame.pack(pady=10, fill='x', padx=20)
    tk.Label(speed_frame, text="Global Speed Multiplier (0.5x - 2.0x)", font=("Helvetica", 10)).pack()
    speed_slider = tk.Scale(speed_frame, from_=0.5, to=2.0, resolution=0.1, orient=tk.HORIZONTAL)
    speed_slider.set(1.0)
    speed_slider.pack(fill='x')

    # Frequency Slider
    freq_frame = tk.Frame(root)
    freq_frame.pack(pady=10, fill='x', padx=20)
    tk.Label(freq_frame, text="Update Frequency (3Hz - 30Hz)", font=("Helvetica", 10)).pack()
    freq_slider = tk.Scale(freq_frame, from_=3, to=30, resolution=1, orient=tk.HORIZONTAL)
    freq_slider.set(30)
    freq_slider.pack(fill='x')
    
    # Servo Speed Checkbox and Slider
    servo_frame = tk.Frame(root)
    servo_frame.pack(pady=10, fill='x', padx=20)
    
    auto_speed_var = tk.BooleanVar(value=True)
    def toggle_auto_speed():
        if auto_speed_var.get():
            servo_slider.config(state=tk.DISABLED)
        else:
            servo_slider.config(state=tk.NORMAL)
            
    tk.Checkbutton(servo_frame, text="Auto-Calculate Max Servo Speed", variable=auto_speed_var, command=toggle_auto_speed, font=("Helvetica", 10)).pack()
    
    tk.Label(servo_frame, text="Manual Servo Speed (Ticks/s)", font=("Helvetica", 10)).pack()
    servo_slider = tk.Scale(servo_frame, from_=10, to=3000, resolution=10, orient=tk.HORIZONTAL, state=tk.DISABLED)
    servo_slider.set(3000)
    servo_slider.pack(fill='x')

    # Joint Indicators
    scales = {}
    joint_names = ["Base (Q/E)", "Shoulder (W/S)", "Elbow (A/D)", "Wrist (Up/Down)", "Gripper (L/R)"]
    
    for i, name in enumerate(joint_names):
        frame = tk.Frame(root)
        frame.pack(pady=5, fill='x', padx=20)
        tk.Label(frame, text=name, width=15, anchor='w').pack(side=tk.LEFT)
        
        joint = Joint(i)
        min_val, max_val = node.joint_limits[joint]
        s = tk.Scale(frame, from_=min_val, to=max_val, resolution=0.01, orient=tk.HORIZONTAL, state=tk.DISABLED)
        s.pack(side=tk.RIGHT, fill='x', expand=True)
        scales[joint] = s

    def update_gui():
        # Update node speed based on user slider
        node.base_speed = speed_slider.get()

        # Update node frequency based on user slider
        new_hz = float(freq_slider.get())
        if new_hz != node.hz:
            node.hz = new_hz
            node.destroy_timer(node.timer)
            node.timer = node.create_timer(1.0 / node.hz, node.control_loop)
            
        # Update auto servo speed toggle and manual speed
        node.auto_servo_speed = auto_speed_var.get()
        node.manual_servo_speed = float(servo_slider.get())

        # Update joint indicators
        for i in range(5):
            joint = Joint(i)
            scales[joint].config(state=tk.NORMAL)
            scales[joint].set(node.cmd_joints[i])
            scales[joint].config(state=tk.DISABLED)
            
        root.after(50, update_gui)

    update_gui()

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
