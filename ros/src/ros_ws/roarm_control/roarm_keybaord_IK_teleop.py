#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import JointState
import tkinter as tk
import threading
import math
import time

class RoArmSafeTeleop(Node):
    def __init__(self):
        super().__init__('roarm_safe_teleop')
        
        self.publisher_ = self.create_publisher(JointState, 'joint_states', 10)
        self.get_logger().info("Safe MEC Teleop Node running. Publishing to /joint_states.")

        self.pressed_keys = {}

        # --- RoArm-M1 Mechanical Constants (mm) ---
        self.LEN_A = 131.22  # Height from ground to shoulder pivot (Z offset)
        self.LEN_B = 140.0   # Length of the bicep
        self.LEN_C = 140.0   # Length of the forearm
        self.LEN_D = 60.0    # Distance from wrist pivot to gripper tip

        # --- Dual State Separation (Cartesian) ---
        # The invisible target driven by the operator
        self.target_x, self.target_y, self.target_z = 277.5, 0.0, 276.5
        self.target_t, self.target_g = 0.0, 0.0 # Pitch & Gripper (Radians)
        
        # The physical commanded state of the arm
        self.cmd_x, self.cmd_y, self.cmd_z = 277.5, 0.0, 276.5
        self.cmd_t, self.cmd_g = 0.0, 0.0 
        self.last_base_rad = 0.0 # Cache for singularity protection

        # --- Velocity Limits ---
        self.operator_speed = 15.0       # mm/s
        self.operator_ang_speed = 0.5    # rad/s
        self.arm_speed = 30.0            # mm/s
        self.arm_ang_speed = 1.0         # rad/s

        # --- Workspace Limits (Spherical Shell) ---
        self.R_MAX = 300.0         
        self.R_MIN = 80.0           
        self.Z_MIN = 25.0          

        # --- Control Loop ---
        self.hz = 30.0
        self.last_time = time.time()
        self.timer = self.create_timer(1.0 / self.hz, self.control_loop)

    def compute_inverse_kinematics(self, x, y, z, pitch):
        """ 
        Direct Python port of the ESP32 RoArm-M1 C++ IK Solver 
        (EoAT_IK -> wigglePlaneIK -> simpleLinkageIK)
        """
        # 1. Base Pan Angle (Protect against singularity at x=0, y=0)
        r_plane = math.sqrt(x**2 + y**2)
        if r_plane > 0.01:
            base_rad = math.atan2(y, x)
            self.last_base_rad = base_rad
        else:
            base_rad = self.last_base_rad
        
        # 2. End of Arm Tooling (EoAT) Offset
        wrist_z = z - (self.LEN_D * math.sin(pitch))
        wrist_r = r_plane - (self.LEN_D * math.cos(pitch))
        
        # 3. Primary Linkage (Law of Cosines)
        z_from_shoulder = wrist_z - self.LEN_A
        d_sq = wrist_r**2 + z_from_shoulder**2
        d = math.sqrt(d_sq)
        
        # Safety clamp to prevent math domain errors (NaN) and division by zero
        if d < 0.001:
            d = 0.001
            d_sq = d**2
        elif d >= (self.LEN_B + self.LEN_C):
            d = self.LEN_B + self.LEN_C - 0.001 
            d_sq = d**2

        cos_elbow = (self.LEN_B**2 + self.LEN_C**2 - d_sq) / (2 * self.LEN_B * self.LEN_C)
        cos_elbow = max(-1.0, min(1.0, cos_elbow)) 
        elbow_rad = math.pi - math.acos(cos_elbow) 
        
        cos_shoulder = (self.LEN_B**2 + d_sq - self.LEN_C**2) / (2 * self.LEN_B * d)
        cos_shoulder = max(-1.0, min(1.0, cos_shoulder))
        
        shoulder_rad = math.acos(cos_shoulder) + math.atan2(z_from_shoulder, wrist_r)
        
        # 4. Wrist Counter-Rotation
        wrist_rad = pitch - (shoulder_rad - elbow_rad)
        gripper_rad = self.cmd_g
        
        return [base_rad, shoulder_rad, elbow_rad, wrist_rad, gripper_rad]

    def control_loop(self):
        current_time = time.time()
        dt = current_time - self.last_time
        self.last_time = current_time

        # --- STEP 1: MOVE THE OPERATOR TARGET ---
        dx_in, dy_in, dz_in = 0.0, 0.0, 0.0
        dt_in, dg_in = 0.0, 0.0

        if self.pressed_keys.get('w'): dx_in += 1
        if self.pressed_keys.get('s'): dx_in -= 1
        if self.pressed_keys.get('a'): dy_in += 1
        if self.pressed_keys.get('d'): dy_in -= 1
        if self.pressed_keys.get('space'): dz_in += 1
        if self.pressed_keys.get('c'):     dz_in -= 1
        
        if self.pressed_keys.get('up'):   dt_in += 1
        if self.pressed_keys.get('down'): dt_in -= 1
        if self.pressed_keys.get('left'):  dg_in += 1
        if self.pressed_keys.get('right'): dg_in -= 1

        self.target_x += dx_in * self.operator_speed * dt
        self.target_y += dy_in * self.operator_speed * dt
        self.target_z += dz_in * self.operator_speed * dt
        self.target_t += dt_in * self.operator_ang_speed * dt
        self.target_g += dg_in * self.operator_ang_speed * dt

        # Clamp angles to prevent integrator windup
        self.target_t = max(-math.pi, min(math.pi, self.target_t))
        self.target_g = max(-math.pi, min(math.pi, self.target_g))

        # Enforce Workspace limits
        if self.target_z < self.Z_MIN: 
            self.target_z = self.Z_MIN
            
        r = math.sqrt(self.target_x**2 + self.target_y**2 + self.target_z**2)
        if r > self.R_MAX:
            scale = self.R_MAX / r
            self.target_x *= scale
            self.target_y *= scale
            self.target_z *= scale
        elif r < self.R_MIN:
            scale = self.R_MIN / r
            self.target_x *= scale
            self.target_y *= scale
            self.target_z *= scale


        # --- STEP 2: ARM CHASES THE TARGET ---
        diff_x = self.target_x - self.cmd_x
        diff_y = self.target_y - self.cmd_y
        diff_z = self.target_z - self.cmd_z
        distance = math.sqrt(diff_x**2 + diff_y**2 + diff_z**2)
        
        max_step = self.arm_speed * dt

        if distance > max_step:
            self.cmd_x += (diff_x / distance) * max_step
            self.cmd_y += (diff_y / distance) * max_step
            self.cmd_z += (diff_z / distance) * max_step
        else:
            self.cmd_x, self.cmd_y, self.cmd_z = self.target_x, self.target_y, self.target_z

        # Pitch Chasing
        diff_t = self.target_t - self.cmd_t
        if abs(diff_t) > self.arm_ang_speed * dt:
            self.cmd_t += math.copysign(self.arm_ang_speed * dt, diff_t)
        else:
            self.cmd_t = self.target_t

        # Gripper Chasing
        diff_g = self.target_g - self.cmd_g
        if abs(diff_g) > self.arm_ang_speed * dt:
            self.cmd_g += math.copysign(self.arm_ang_speed * dt, diff_g)
        else:
            self.cmd_g = self.target_g


        # --- STEP 3: SOLVE IK & PUBLISH TO ROS 2 ---
        joint_radians = self.compute_inverse_kinematics(
            self.cmd_x, self.cmd_y, self.cmd_z, self.cmd_t
        )

        msg = JointState()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.name = ['base', 'shoulder', 'elbow', 'pitch', 'gripper']
        msg.position = joint_radians
        
        # Pass the velocity down to the VHC bridge for the JSON 'S' parameters
        vel_cmd = self.arm_speed 
        msg.velocity = [vel_cmd, vel_cmd, vel_cmd, vel_cmd, vel_cmd]

        self.publisher_.publish(msg)

def run_tkinter_gui(node):
    root = tk.Tk()
    root.title("RoArm-M1 Safe Teleop")
    root.geometry("350x140")

    text = ("Mode: CARTESIAN (IK Offloaded)\n\n"
            "WASD: XY Plane | Space/C: Height\n"
            "Arrows: Pitch & Gripper")
            
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
    node = RoArmSafeTeleop()
    
    # Run ROS 2 spin in a background thread so Tkinter can run on the main thread
    ros_thread = threading.Thread(target=rclpy.spin, args=(node,), daemon=True)
    ros_thread.start()
    
    run_tkinter_gui(node)

if __name__ == '__main__':
    main()