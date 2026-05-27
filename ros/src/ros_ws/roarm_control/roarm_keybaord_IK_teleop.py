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
        self.LEN_A = 115.432
        self.LEN_B = 41.0513
        self.LEN_C = 168.8579
        self.LEN_D = 127.9234
        self.LEN_E = 108.5357
        self.LEN_F = 7.7076
        self.LEN_G = 90.0
        self.LEN_H = -13.75

        # --- Dual State Separation (Cartesian) ---
        # The invisible target driven by the operator
        self.target_x, self.target_y, self.target_z = 277.5, -13.75, 276.5
        self.target_t, self.target_g = 90.0, 0.0 # Pitch (Degrees) & Gripper (Radians)
        
        # The physical commanded state of the arm
        self.cmd_x, self.cmd_y, self.cmd_z = 277.5, -13.75, 276.5
        self.cmd_t, self.cmd_g = 90.0, 0.0 
        self.last_base_rad = 0.0 # Cache for singularity protection

        # --- Velocity Limits ---
        self.operator_speed = 15.0       # mm/s
        self.operator_ang_speed = 15.0   # degrees/s for pitch
        self.arm_speed = 30.0            # mm/s
        self.arm_ang_speed = 30.0        # degrees/s for pitch

        # --- Workspace Limits (Spherical Shell) ---
        self.R_MAX = 300.0         
        self.R_MIN = 80.0           
        self.Z_MIN = 25.0          

        # --- Control Loop ---
        self.hz = 30.0
        self.last_time = time.time()
        self.timer = self.create_timer(1.0 / self.hz, self.control_loop)

    def compute_inverse_kinematics(self, x, y, z, pitch_deg):
        """ 
        Direct Python port of the ESP32 RoArm-M1 C++ IK Solver 
        (wigglePlaneIK -> EoAT_IK -> simpleLinkageIK)
        """
        # --- 1. wigglePlaneIK ---
        aIn = x
        bIn = -y
        if bIn > 0:
            L2C = aIn * aIn + bIn * bIn
            LC = math.sqrt(L2C)
            lambda_ = math.degrees(math.atan2(aIn, bIn))
            psi = math.degrees(math.acos(self.LEN_H/LC)) if LC >= abs(self.LEN_H) else 0
            LB = math.sqrt(L2C - self.LEN_H * self.LEN_H) if L2C >= self.LEN_H * self.LEN_H else 0
            alpha = psi + lambda_ - 90
        elif bIn == 0:
            alpha = 90 + math.degrees(math.asin(self.LEN_H/aIn)) if aIn != 0 else 90
            L2C = aIn * aIn + bIn * bIn
            LB = math.sqrt(L2C)
        else: # bIn < 0
            bIn = -bIn
            L2C = aIn * aIn + bIn * bIn
            LC = math.sqrt(L2C)
            lambda_ = math.degrees(math.atan2(aIn, bIn))
            psi = math.degrees(math.acos(self.LEN_H/LC)) if LC >= abs(self.LEN_H) else 0
            LB = math.sqrt(L2C - self.LEN_H * self.LEN_H) if L2C >= self.LEN_H * self.LEN_H else 0
            alpha = 90 - lambda_ + psi
            
        angle_1 = alpha + 90
        len_totalXY = LB - self.LEN_B

        # --- 2. EoAT_IK ---
        if pitch_deg == 90:
            betaGenOut = pitch_deg - self.LEN_G
            betaRad = math.radians(betaGenOut)
            angleRad = math.radians(pitch_deg)
            aGenOut = self.LEN_E
            bGenOut = self.LEN_F
        elif pitch_deg < 90:
            betaGenOut = 90 - pitch_deg
            betaRad = math.radians(betaGenOut)
            angleRad = math.radians(pitch_deg)
            aGenOut = math.cos(angleRad)*self.LEN_F + math.cos(betaRad)*self.LEN_E
            bGenOut = math.sin(angleRad)*self.LEN_F - math.sin(betaRad)*self.LEN_E
            betaGenOut = -betaGenOut
        else: # pitch_deg > 90
            betaGenOut = self.LEN_G - (180 - pitch_deg)
            betaRad = math.radians(betaGenOut)
            angleRad = math.radians(pitch_deg)
            aGenOut = -math.cos(math.pi-angleRad)*self.LEN_F + math.cos(betaRad)*self.LEN_E
            bGenOut = math.sin(math.pi-angleRad)*self.LEN_F + math.sin(betaRad)*self.LEN_E

        angle_EoAT = betaGenOut
        len_a = aGenOut
        len_b = bGenOut

        # --- 3. simpleLinkageIK ---
        LA = self.LEN_C
        LB = self.LEN_D
        aIn2 = len_totalXY - len_a
        bIn2 = z - self.LEN_A + len_b

        # Safety clamps for math domain
        if aIn2 < 0.001: aIn2 = 0.001
        
        if bIn2 == 0:
            val = (LA * LA + aIn2 * aIn2 - LB * LB) / (2 * LA * aIn2)
            psi = math.degrees(math.acos(max(-1.0, min(1.0, val))))
            alpha2 = 90 - psi
            val2 = (aIn2 * aIn2 + LB * LB - LA * LA) / (2 * aIn2 * LB)
            omega = math.degrees(math.acos(max(-1.0, min(1.0, val2))))
            beta = psi + omega
        else:
            L2C2 = aIn2 * aIn2 + bIn2 * bIn2
            LC2 = math.sqrt(L2C2)
            lambda2 = math.degrees(math.atan2(bIn2, aIn2))
            val = (LA * LA + L2C2 - LB * LB) / (2 * LA * LC2)
            psi = math.degrees(math.acos(max(-1.0, min(1.0, val))))
            alpha2 = 90 - lambda2 - psi
            val2 = (LB * LB + L2C2 - LA * LA) / (2 * LC2 * LB)
            omega = math.degrees(math.acos(max(-1.0, min(1.0, val2))))
            beta = psi + omega

        delta = 90 - alpha2 - beta
        
        angle_2 = alpha2
        angle_3 = beta
        angle_IKE = delta
        
        # --- 4. Final Combination ---
        angle_4 = angle_IKE + angle_EoAT
        
        # --- 5. Map to ROS 2 joint states ---
        base_rad     = math.radians(180.0 - angle_1)
        shoulder_rad = math.radians(45.0 - angle_2)
        elbow_rad    = math.radians(-angle_3)
        wrist_rad    = math.radians(angle_4)
        gripper_rad  = self.cmd_g
        
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
        self.target_t = max(0.0, min(180.0, self.target_t)) # Pitch in degrees
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