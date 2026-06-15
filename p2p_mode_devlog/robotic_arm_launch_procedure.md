# Robotic Arm Offloading — Teleop Launch Procedure

This document describes how to start the Modular Gateway for the robotic arm VHC→MEC offloading use case, specifically focusing on the remote teleoperation control modes.

## Architecture Recap

The **VHC (Raspberry Pi)** acts as a hardware proxy, running the official Waveshare `serial_ctrl_py` driver to talk to the physical arm. 
The **MEC (Laptop/Server)** runs the computationally heavy control interfaces (IK Solvers or Direct Joint Math) and the GUI.

| Handler | VHC Role | MEC Role | Topic (Global) |
| :--- | :--- | :--- | :--- |
| `SerialFeedbackHandler` (14) | **TX** (Send) | **RX** (Receive) | `/serial_ctrl/rx` |
| `SerialStringHandler` (12) | **RX** (Receive) | **TX** (Send) | `/serial_ctrl/tx` |
| `JointStateHandler` (13) | **RX** (Receive) | **TX** (Send) | `/joint_states` |

---

## 1. Setup the MEC (Laptop) — Docker Mode

The MEC runs inside a Docker container with GUI support enabled for the Tkinter control interface.

### Step 1: Enable X11 Access
Allow the Docker container to display windows on your host screen. This is **required** for the keyboard teleop GUI to appear:
```bash
xhost +local:docker
```

### Step 2: Launch the MEC Gateway & Teleop Node
*   `TELEOP=true` tells the MEC to launch the keyboard control GUI instead of the standard RViz sliders.
*   `IK_MEC` determines which mathematical control mode to use.
*   `SKIP_BUILD` (optional) can be set to `0` to force a rebuild of the workspace inside the container. By default, it is set to `1` in `docker-compose.robotic_arm_mec.yml` to bypass the compilation phase for faster startups.

**Mode A: Direct Joint Control (Recommended)**
Manually control each of the 5 joints independently.
```bash
# From the repository root
TELEOP=true IK_MEC=false docker compose -f docker-compose.robotic_arm_mec.yml up --build
```

**Mode B: Inverse Kinematics (Cartesian) Control**
Move the arm in X/Y/Z space using an exact port of the ESP32's geometric IK solver.
```bash
# From the repository root
TELEOP=true IK_MEC=true docker compose -f docker-compose.robotic_arm_mec.yml up --build
```

---

## 2. Setup the VHC (Raspberry Pi) — Bare Metal

The Pi runs the gateway natively to allow direct access to the USB serial port. Because all our teleop modes now output standard `JointState` messages, the Pi setup is fully standardized.

### Step 1: Build and Source
```bash
# Source the main ROS 2 installation first
source /opt/ros/jazzy/setup.bash

cd ~/RobotArmProject/ROS2_modular_edge_offloading/ros/src/ros_ws
colcon build --packages-select modular_gateway_sender serial_ctrl
source install/setup.bash
```

> [!NOTE]
> **Duplicate Package Error during build?** If you mounted the old `RoArm-M1` repository into your container alongside the forked `serial_ctrl` package, `colcon build` will complain about duplicate package names. Simply create an empty `COLCON_IGNORE` file inside the old `RoArm-M1/src/serial_ctrl/` folder to tell ROS to ignore the duplicate.

### Step 2: Start the VHC Gateway
Launch the gateway node using `robotic_arm_vhc_launch.py`:

```bash
ros2 launch modular_gateway_sender robotic_arm_vhc_launch.py \
  operation_mode:=p2p \
  peer_host:=12.1.1.67
```

### Step 3: Start the Hardware Driver
In another terminal on the Pi, ensure your `serial_ctrl` node is running. This **must** be launched in a separate terminal to avoid serial port blocking issues:
```bash
# Source the main ROS 2 installation
source /opt/ros/jazzy/setup.bash

# Source the project workspace (where the forked serial_ctrl now lives)
source ~/RobotArmProject/ROS2_modular_edge_offloading/ros/src/ros_ws/install/setup.bash
ros2 run serial_ctrl serial_ctrl
```

---

## Control Layouts

### Direct Joint Control (`IK_MEC=false`)
*   **Q/E**: Base Rotation
*   **W/S**: Shoulder
*   **A/D**: Elbow
*   **Up/Down**: Wrist
*   **Left/Right**: Gripper (half speed)

### Cartesian IK Control (`IK_MEC=true`)
*   **W/S**: Move Forward/Backward (X-axis)
*   **A/D**: Move Left/Right (Y-axis)
*   **Space/C**: Move Up/Down (Z-axis)
*   **Up/Down**: Pitch 
*   **Left/Right**: Gripper 

---

## Troubleshooting & Verification

### Verify Connectivity
If you don't see movement, verify data is flowing by echoing topics:
- **On Pi**: `ros2 topic echo /joint_states` (should see messages streaming from laptop)
- **On Laptop**: `ros2 topic echo /serial_ctrl/rx` (should see feedback from arm)

### Log Levels
Internal gateway logs are now in **TEXT mode** on the console. Look for `[gateway]` lines to confirm:
- `Successfully connected to peer MEC`
- `Activated handler for msgType 13` (JointState)
