#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import String
import tkinter as tk
import threading
import json
import math

class RoArmKeyboardTeleop(Node):
    def __init__(self):
        super().__init__('roarm_keyboard_teleop')
        
        # Publisher for transmit topic
        self.publisher_ = self.create_publisher(String, '/serial_ctrl/tx', 10)
        self.get_logger().info("MEC Teleop Node running. Publishing JSON to /serial_ctrl/tx")

        self.mode = "CARTESIAN"
        self.pressed_keys = {}

        self.cur_x = 180.0
        self.cur_y = 0.0
        self.cur_z = 120.0
        self.cur_t = 0.0   
        self.cur_g = 0.0   

        self.joints = {1: 2047, 2: 2047, 3: 2047, 4: 2047, 5: 2047}
        self.selected_joint = 1

        self.linear_step = 2.5     
        self.angular_step = 0.04   
        self.joint_step = 20       

        self.R_MAX = 300.0         
        self.R_MIN = 80.0           
        self.Z_MIN = 25.0          

        self.timer = self.create_timer(1.0 / 30.0, self.update_loop)

    def update_loop(self):
        if self.mode == "CARTESIAN":
            self.process_cartesian()
        elif self.mode == "JOINT":
            self.process_joint()

    def process_cartesian(self):
        state_changed = False
        
        if self.pressed_keys.get('w'): self.cur_x += self.linear_step; state_changed = True
        if self.pressed_keys.get('s'): self.cur_x -= self.linear_step; state_changed = True
        if self.pressed_keys.get('a'): self.cur_y += self.linear_step; state_changed = True
        if self.pressed_keys.get('d'): self.cur_y -= self.linear_step; state_changed = True
        
        if self.pressed_keys.get('space'): self.cur_z += self.linear_step; state_changed = True
        if self.pressed_keys.get('c'):     self.cur_z -= self.linear_step; state_changed = True
        
        if self.pressed_keys.get('up'):   self.cur_t += self.angular_step; state_changed = True
        if self.pressed_keys.get('down'): self.cur_t -= self.angular_step; state_changed = True
        
        if self.pressed_keys.get('left'):  self.cur_g += self.angular_step; state_changed = True
        if self.pressed_keys.get('right'): self.cur_g -= self.angular_step; state_changed = True

        if state_changed:
            if self.cur_z < self.Z_MIN:
                self.cur_z = self.Z_MIN

            r = math.sqrt(self.cur_x**2 + self.cur_y**2 + self.cur_z**2)
            if r > self.R_MAX:
                scale = self.R_MAX / r
                self.cur_x *= scale; self.cur_y *= scale; self.cur_z *= scale
            elif r < self.R_MIN:
                scale = self.R_MIN / r
                self.cur_x *= scale; self.cur_y *= scale; self.cur_z *= scale

            payload = {
                "T": 104,
                "x": round(self.cur_x, 1),
                "y": round(self.cur_y, 1),
                "z": round(self.cur_z, 1),
                "t": round(self.cur_t, 2),
                "g": round(self.cur_g, 2),
                "spd": 0
            }
            self.publish_command(payload)

    def process_joint(self):
        state_changed = False

        for i in range(1, 6):
            if self.pressed_keys.get(str(i)):
                self.selected_joint = i

        if self.pressed_keys.get('w'): self.joints[self.selected_joint] += self.joint_step; state_changed = True
        if self.pressed_keys.get('s'): self.joints[self.selected_joint] -= self.joint_step; state_changed = True

        if state_changed:
            self.joints[self.selected_joint] = max(0, min(4095, self.joints[self.selected_joint]))

            payload = {
                "T": 101,
                "joint": self.selected_joint,
                "value": self.joints[self.selected_joint],
                "spd": 0
            }
            self.publish_command(payload)

    def publish_command(self, data_dict):
        msg = String()
        msg.data = json.dumps(data_dict) + "\n"
        self.publisher_.publish(msg)

def run_tkinter_gui(node):
    root = tk.Tk()
    root.title("RoArm-M1 Teleop Pad (MEC)")
    root.geometry("350x150")

    label = tk.Label(root, text="Mode: CARTESIAN\n\nWASD: XY Plane | Space/C: Height\nArrows: Pitch & Gripper\nPress [M] to Switch Modes", font=("Helvetica", 11))
    label.pack(pady=20)

    def key_press(event):
        key = event.keysym.lower()
        node.pressed_keys[key] = True
        
        if key == 'm':
            node.mode = "JOINT" if node.mode == "CARTESIAN" else "CARTESIAN"
            if node.mode == "JOINT":
                label.config(text=f"Mode: JOINT CONTROL\n\nKeys 1-5: Choose Joint (Active: {node.selected_joint})\nW/S: Move Selected Joint\nPress [M] to Switch Modes")
            else:
                label.config(text="Mode: CARTESIAN\n\nWASD: XY Plane | Space/C: Height\nArrows: Pitch & Gripper\nPress [M] to Switch Modes")

    def key_release(event):
        key = event.keysym.lower()
        node.pressed_keys[key] = False

    root.bind("<KeyPress>", key_press)
    root.bind("<KeyRelease>", key_release)
    
    def on_closing():
        root.destroy()
        rclpy.shutdown()

    root.protocol("WM_DELETE_WINDOW", on_closing)
    root.mainloop()

def main(args=None):
    rclpy.init(args=args)
    node = RoArmKeyboardTeleop()
    ros_thread = threading.Thread(target=rclpy.spin, args=(node,), daemon=True)
    ros_thread.start()
    run_tkinter_gui(node)

if __name__ == '__main__':
    main()