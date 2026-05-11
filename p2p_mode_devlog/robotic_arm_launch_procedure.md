# Robotic Arm Offloading — Launch Procedure

This document describes how to start the Modular Gateway for the robotic arm VHC→MEC offloading use case. Both P2P modes are covered.

## Architecture Recap

The VHC (Raspberry Pi UE) acts as a USB hardware proxy. The MEC (5G Edge Server) runs the compute application. All data flows through the Modular Gateway data plane.

| Handler | VHC Role | MEC Role | Topic |
| :--- | :--- | :--- | :--- |
| `SerialFeedbackHandler` (14) | **TX** — reads from arm, sends Uplink | **RX** — publishes to compute app | `serial_ctrl/rx` |
| `SerialStringHandler` (12) | **RX** — publishes to arm | **TX** — compute app sends, Downlink | `serial_ctrl/tx` |
| `JointStateHandler` (13) | **RX** — publishes to arm | **TX** — compute app sends, Downlink | `joint_states` |

Task config file: `config/robotic_arm_config.json`

---

## Prerequisites

### MEC (Docker)
- Docker and Docker Compose installed on the edge server.
- Port `7401` open on the MEC host firewall (data plane).

### VHC (Bare Metal)
- ROS 2 Jazzy installed and workspace built:
  ```bash
  cd ~/ros_ws && colcon build --packages-select modular_gateway_sender
  source install/setup.bash
  ```

---

## Mode 1: `p2p` — Static Direct Connection

Both nodes use fixed IP addresses. No discovery service is required. The MEC starts first and listens; the VHC then connects.

**Step 1 — Start the MEC (Docker):**
```bash
cd autonomous-driving-ros2/ros

# Default mode is p2p. Edit the compose file to set MEC_LISTEN_PORT if needed.
docker compose -f docker-compose.robotic_arm_mec.yml up
```
Or override inline:
```bash
OPERATION_MODE=p2p MEC_LISTEN_PORT=7401 \
  docker compose -f docker-compose.robotic_arm_mec.yml up
```

**Step 2 — Start the VHC (bare metal):**
```bash
ros2 launch modular_gateway_sender robotic_arm_vhc_launch.py \
  operation_mode:=p2p \
  peer_host:=<MEC_IP_ADDRESS> \
  peer_port:=7401
```

---

## Mode 2: `p2p_ds` — Discovery-Assisted Connection

The MEC registers with the Discovery Service. The VHC uses the same Discovery Service to resolve the MEC's IP and connect. A running `DiscoveryService` instance is required.

**Step 1 — Start the MEC (Docker):**
```bash
cd autonomous-driving-ros2/ros

OPERATION_MODE=p2p_ds DISCOVERY_HOST=<DS_IP> DISCOVERY_PORT=9090 \
  docker compose -f docker-compose.robotic_arm_mec.yml up
```

**Step 2 — Start the VHC (bare metal):**
```bash
ros2 launch modular_gateway_sender robotic_arm_vhc_launch.py \
  operation_mode:=p2p_ds \
  discovery_host:=<DS_IP> \
  discovery_port:=9090
```

---

## Optional Launch Arguments

All arguments below can be appended to either launch command.

| Argument | Default | Description |
| :--- | :--- | :--- |
| `operation_mode` | `p2p` | `p2p` or `p2p_ds` |
| `peer_host` | `192.168.1.100` | `[p2p]` Static IP of the MEC |
| `peer_port` | `7401` | `[p2p]` Data plane port of the MEC |
| `discovery_host` | `192.168.1.1` | `[p2p_ds]` Discovery service IP |
| `discovery_port` | `9090` | `[p2p_ds]` Discovery service port |
| `group_id` | `60` (VHC) / `61` (MEC) | Component group ID |
| `id_in_group` | `1` | Component ID within the group |
| `local_config_path` | `config/robotic_arm_config.json` | Path to the task config JSON |

---

## Expected Startup Log Sequence

A successful startup on the VHC will produce logs in this order:

1.  `GatewayController: Constructing...` — Parameters read.
2.  `GatewayController: State: INITIALIZING` — Transport created.
3.  *(p2p_ds only)* `Discovery client started. Waiting for response...`
4.  `GatewayController: State: WAITING_FOR_DP_CONNECTION` — Attempting TCP connect to MEC.
5.  `[P2P] Successfully connected to peer MEC.` — Data plane established.
6.  `Data plane confirmed. Gateway is now OPERATIONAL` — Handlers activating.
7.  `Activated handler for msgType 12 with mode 1` — `SerialStringHandler` (PUBLISHER_ONLY) active.
8.  `Activated handler for msgType 13 with mode 1` — `JointStateHandler` (PUBLISHER_ONLY) active.
9.  `Activated handler for msgType 14 with mode 0` — `SerialFeedbackHandler` (SUBSCRIBER_ONLY) active.
