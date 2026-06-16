#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState, Joy
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
        
        self.joy_axes = [0.0] * 8
        self.lt_touched = False
        self.rt_touched = False
        self.joy_sub = self.create_subscription(Joy, '/joy', self.joy_callback, 10)
        
        self.get_logger().info("Direct Joint Teleop Node running. Publishing to /joint_states.")

        self.pressed_keys = {}

        # The commanded joint states (radians)
        self.target_joints = [0.0, 0.0, 0.0, 0.0, 0.0]
        self.cmd_joints = [0.0, 0.0, 0.0, 0.0, 0.0]
        self.last_published_joints = [None, None, None, None, None]

        # Auto Servo Speed feature
        self.auto_servo_speed = True
        self.manual_servo_speed = 3000.0
        self.acceleration_cap = 60.0

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

    def joy_callback(self, msg):
        self.joy_axes = msg.axes
        if len(self.joy_axes) >= 6:
            if self.joy_axes[2] != 0.0: self.lt_touched = True
            if self.joy_axes[5] != 0.0: self.rt_touched = True

    def control_loop(self):
        current_time = time.time()
        dt = current_time - self.last_time
        self.last_time = current_time

        kb_d_joints = [0.0, 0.0, 0.0, 0.0, 0.0]
        joy_d_joints = [0.0, 0.0, 0.0, 0.0, 0.0]

        if self.pressed_keys.get('q'): kb_d_joints[0] += 1.0
        if self.pressed_keys.get('e'): kb_d_joints[0] -= 1.0
        
        if self.pressed_keys.get('w'): kb_d_joints[1] += 1.0
        if self.pressed_keys.get('s'): kb_d_joints[1] -= 1.0
        
        if self.pressed_keys.get('d'): kb_d_joints[2] += 1.0
        if self.pressed_keys.get('a'): kb_d_joints[2] -= 1.0
        
        if self.pressed_keys.get('up'):    kb_d_joints[3] += 1.0
        if self.pressed_keys.get('down'):  kb_d_joints[3] -= 1.0
        
        if self.pressed_keys.get('left'):  kb_d_joints[4] += 1.0
        if self.pressed_keys.get('right'): kb_d_joints[4] -= 1.0

        # Blend Joystick Input
        if len(self.joy_axes) >= 6:
            # Base (Pan): Left Stick L/R (Axis 0)
            joy_d_joints[0] += self.joy_axes[0]
            # Shoulder (Lift): Left Stick U/D (Axis 1)
            joy_d_joints[1] += self.joy_axes[1]
            # Elbow (Extension): Right Stick U/D (Axis 4)
            joy_d_joints[2] += self.joy_axes[4]
            # Wrist (Pitch): Right Stick L/R (Axis 3)
            joy_d_joints[3] += self.joy_axes[3]
            
            # Gripper: Sum of Triggers (Axis 2 and 5)
            # ROS joy driver outputs 1.0 (unpressed) to -1.0 (fully pressed) for triggers
            # Linux joy workaround: Untouched triggers default to 0.0, which acts as 50% pressed!
            lt_axis = self.joy_axes[2] if self.lt_touched else 1.0
            rt_axis = self.joy_axes[5] if self.rt_touched else 1.0
            
            lt_val = (1.0 - lt_axis) / 2.0
            rt_val = (1.0 - rt_axis) / 2.0
            joy_d_joints[4] += (lt_val - rt_val)

        for i in range(5):
            joint = Joint(i)
            # Keyboard speed uses the joint multipliers, Joystick speed ignores them
            kb_speed = self.base_speed * self.joint_speed_multipliers[joint]
            joy_speed = self.base_speed
                
            delta_rad = (kb_d_joints[i] * kb_speed) + (joy_d_joints[i] * joy_speed)
            self.target_joints[i] += delta_rad * dt
            
            # Clamp to limits
            min_val, max_val = self.joint_limits[joint]
            self.target_joints[i] = max(min_val, min(max_val, self.target_joints[i]))

            # Chase target (smooth movement)
            # We chase at joy_speed (which is self.base_speed) so it can catch up 
            # to both joystick targets and slower keyboard targets smoothly.
            diff = self.target_joints[i] - self.cmd_joints[i]
            step = joy_speed * dt
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

            # Pack acceleration cap into unused effort array
            msg.effort = [self.acceleration_cap] * 5

            self.publisher_.publish(msg)
            self.last_published_joints = self.cmd_joints.copy()

def run_tkinter_gui(node):
    root = tk.Tk()
    root.title("RoArm-M1 Direct Joint Teleop")
    root.geometry("800x650")

    main_frame = tk.Frame(root)
    main_frame.pack(fill='both', expand=True, padx=10, pady=10)
    
    left_col = tk.Frame(main_frame)
    left_col.pack(side=tk.LEFT, fill='both', expand=True, padx=(0, 10))
    
    right_col = tk.Frame(main_frame)
    right_col.pack(side=tk.RIGHT, fill='both', expand=True, padx=(10, 0))

    text = ("Mode: DIRECT JOINT CONTROL")
            
    label = tk.Label(left_col, text=text, font=("Helvetica", 12, "bold"))
    label.pack(pady=10)

    # Speed Slider
    speed_frame = tk.Frame(left_col)
    speed_frame.pack(pady=10, fill='x', padx=20)
    tk.Label(speed_frame, text="Global Speed Multiplier (0.5x - 2.0x)", font=("Helvetica", 10)).pack()
    speed_slider = tk.Scale(speed_frame, from_=0.5, to=2.0, resolution=0.1, orient=tk.HORIZONTAL)
    speed_slider.set(1.0)
    speed_slider.pack(fill='x')

    # Frequency Slider
    freq_frame = tk.Frame(left_col)
    freq_frame.pack(pady=10, fill='x', padx=20)
    tk.Label(freq_frame, text="Update Frequency (3Hz - 30Hz)", font=("Helvetica", 10)).pack()
    freq_slider = tk.Scale(freq_frame, from_=3, to=30, resolution=1, orient=tk.HORIZONTAL)
    freq_slider.set(30)
    freq_slider.pack(fill='x')
    
    # Servo Speed Checkbox and Slider
    servo_frame = tk.Frame(left_col)
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

    # Servo Acceleration Slider
    accel_frame = tk.Frame(left_col)
    accel_frame.pack(pady=10, fill='x', padx=20)
    tk.Label(accel_frame, text="Acceleration Cap (1-60)", font=("Helvetica", 10)).pack()
    accel_slider = tk.Scale(accel_frame, from_=1, to=60, resolution=1, orient=tk.HORIZONTAL)
    accel_slider.set(60)
    accel_slider.pack(fill='x')

    # Joint Indicators
    scales = {}
    joint_names = ["Base (Q/E)", "Shoulder (W/S)", "Elbow (A/D)", "Wrist (Up/Down)", "Gripper (L/R)"]
    
    tk.Label(left_col, text="Virtual Joint State", font=("Helvetica", 12, "bold")).pack(pady=(20, 5))
    
    for i, name in enumerate(joint_names):
        frame = tk.Frame(left_col)
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
        
        # Update acceleration cap
        node.acceleration_cap = float(accel_slider.get())

        # Update joint indicators
        for i in range(5):
            joint = Joint(i)
            scales[joint].config(state=tk.NORMAL)
            scales[joint].set(node.cmd_joints[i])
            scales[joint].config(state=tk.DISABLED)
            
        # Update joy indicators
        if len(node.joy_axes) >= 6:
            for i in range(6):
                joy_scales[i].config(state=tk.NORMAL)
                joy_scales[i].set(node.joy_axes[i])
                joy_scales[i].config(state=tk.DISABLED)
            
        root.after(50, update_gui)

    # Joy Indicators (Right Column)
    tk.Label(right_col, text="Joystick Activity", font=("Helvetica", 12, "bold")).pack(pady=10)
    joy_scales = []
    joy_labels = ["LS L/R (Base)", "LS U/D (Shoulder)", "LT (Gripper Close)", "RS L/R (Wrist)", "RS U/D (Elbow)", "RT (Gripper Open)"]
    for i, name in enumerate(joy_labels):
        jf = tk.Frame(right_col)
        jf.pack(pady=15, fill='x')
        tk.Label(jf, text=name, width=15, anchor='w').pack(side=tk.LEFT)
        s = tk.Scale(jf, from_=-1.0, to=1.0, resolution=0.01, orient=tk.HORIZONTAL, state=tk.DISABLED)
        s.pack(side=tk.RIGHT, fill='x', expand=True)
        joy_scales.append(s)

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
