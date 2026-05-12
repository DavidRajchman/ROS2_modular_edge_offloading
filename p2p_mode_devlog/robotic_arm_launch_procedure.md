# Robotic Arm Offloading — Launch Procedure

This document describes how to start the Modular Gateway for the robotic arm VHC→MEC offloading use case.

## Architecture Recap

The **VHC (Raspberry Pi)** acts as a hardware proxy, reading from and writing to the physical arm. The **MEC (Laptop/Server)** runs the compute application and the GUI sliders.

| Handler | VHC Role | MEC Role | Topic (Global) |
| :--- | :--- | :--- | :--- |
| `SerialFeedbackHandler` (14) | **TX** (Send) | **RX** (Receive) | `/serial_ctrl/rx` |
| `SerialStringHandler` (12) | **RX** (Receive) | **TX** (Send) | `/serial_ctrl/tx` |
| `JointStateHandler` (13) | **RX** (Receive) | **TX** (Send) | `/joint_states` |

---

## 1. Setup the MEC (Laptop) — Docker Mode

The MEC runs inside a Docker container with GUI support enabled for RViz and Sliders.

### Step 1: Enable X11 Access
Allow the Docker container to display windows on your host screen:
```bash
xhost +local:docker
```

### Step 2: Start the MEC Gateway
Launch the container. It will automatically build the `RoArm-M1` code and start the gateway:
```bash
# From the repository root
docker compose -f docker-compose.robotic_arm_mec.yml up --build
```

### Step 3: Launch the Sliders (GUI)
In a **new terminal** on your laptop, enter the running container and start the control interface:
```bash
# Enter the container
docker exec -it robotic_arm_mec bash

# Inside the container:
source install/setup.bash
ros2 launch roarm roarm.launch.py gui:=True
```

---

## 2. Setup the VHC (Raspberry Pi) — Bare Metal

The Pi runs the gateway natively to allow direct access to the USB serial port.

### Step 1: Build and Source
```bash
cd ~/RobotArmProject/ROS2_modular_edge_offloading/ros/src/ros_ws
colcon build --packages-select modular_gateway_sender
source install/setup.bash
```

### Step 2: Start the VHC Gateway
```bash
ros2 launch modular_gateway_sender robotic_arm_vhc_launch.py \
  operation_mode:=p2p \
  peer_host:=<MEC_IP_ADDRESS>
```

### Step 3: Start the Hardware Driver
In another terminal on the Pi, ensure your `serial_ctrl` node is running to bridge ROS topics to the USB port:
```bash
# Assuming RoArm-M1 is built on the Pi
source ~/RobotArmProject/RoArm-M1/install/setup.bash
ros2 run serial_ctrl serial_ctrl_py
```

---

## Troubleshooting & Verification

### Verify Connectivity
If you don't see movement, verify data is flowing by echoing topics:
- **On Pi**: `ros2 topic echo /joint_states` (should see messages from laptop)
- **On Laptop**: `ros2 topic echo /serial_ctrl/rx` (should see feedback from arm)

### Log Levels
Internal gateway logs are now in **TEXT mode** on the console. Look for `[gateway]` lines to confirm:
- `Successfully connected to peer MEC`
- `Activated handler for msgType 14`
