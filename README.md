# Autonomous Driving ROS2 - Ofloading Manager


## Getting started
This branch is focused on creating the offloading manager aplication and all acompaniying components.
Special atention was put into making it possible to merge into the main branch. The only files not directly related to the offloading project that could cause merge conflicts are the [DOCKERFILES] and [docker-compose.yml](./docker-compose.yml) However the root location of all docker containers has been kept. So only aditions to those files should be required for succesfull merge, no refactorization is expected to be needed.

## The basic idea of the offloading system:
It should manage transparently sending data from an autonomous vehicle [VHC] runing the ros2 enviroment to its virtual copy [MEC] hosted on an mobule edge server. Except for adding the controls of the offloading to the requesting VHC no other modifications of ros nodes are needed to achieve the offloading.

Not all reqeust for offloading can be granted due to sevaral causes (network conditions, edge server utilization etc.) For this reason an offloading manager [OM] Is coardinating the entire system. Its also posible to have more than 1 MEC containers per VHC due to the adition of an [Bridge] components which acts as an mobile edge based data router. This allows for deployment of task specific computing solutions (eg. a GPU oriented server and a general computing server)

Due to the system ability to be deployed on a mobile network, it is designed not to need fixed IP adresses. For this reason a discovery service [DISC] component has been aded to the system. It is the only component with a fixed IP adress, and this adress **needs to be changed inside the code** if the system deployes in a diferent network enviroments. Please refer to the APPENDIX A for the location of the DISC IP address per component.

## Architecture of a simple of an minimal offloading testing experiment
The current design of all components requires that the Bridge, DISC and OM components do not share the same network interface and are not reachable by each other on the localhost network. Every one of those components must have an IP adress of its own. This can be achieved with a simple docker bridge network. 

If using multiple computers is needed for testing the offloading in a realworld scenario, the simplest solution is to use dedicated HW for those 3 restricted components. However an untested solution using macvlan has beed implemented and is able to launch (but there were isues with network routing, likely unrelated to the macvlan itself.) See [docker-compose.macvlan.override.yml](./docker-compose.macvlan.override.yml) for more information on how to setup the macvlan

Here is a list of containers that need to be started and a brief description of their functionality
- **offloading-manager** image runs the offloading manager for the offloading system. It is responsible for coordinating the offloading process and making decisions about which tasks to offload. It contains the global configuration ([OffloadingManager/config.json](./OffloadingManager/config.json)) for the offloading system which lists all configurable experiments. It also contains the algorithm ([OffloadingManager/algorithm.py](./OffloadingManager/algorithm.py)) which is the only file that should be edited during research. It contains OM configuration as well as the actual offloading algorithm (single persistent thread). Currently an autoapprove placeholder is present. Dockerfile: [bridge/Dockerfile](./bridge/Dockerfile)
- **discovery** image runs the discovery service for the offloading system. It is responsible for service registration and discovery. It also distributes the global configuration to other components. Dockerfile: [DiscoveryService/Dockerfile](./DiscoveryService/Dockerfile)
- **bridge** image acts as a mobile edge based data router. It is responsible for routing data between the VHC and MEC containers. Dockerfile: [bridge/Dockerfile](./bridge/Dockerfile)
- **ros2_vhc** image runs the ROS2 environment for the autonomous vehicle. Dockerfile: [ros/Dockerfile](./ros/Dockerfile)
- **ros2_mec** image runs the ROS2 environment for the virtual copy of the VHC. It waits for offloading requests to be accepted. Utilizes the same dockerfile as VHC. Dockerfile: [ros/Dockerfile](./ros/Dockerfile)

## Network architecture
The entire system is connected through the bridge component (except for the discovery service and its direct connection to all components). There are 2 separeted parts of every aplication on diferent sockets - data plane and control plane. Data plane is solely reserved for data transfer, while control plane is used for signaling comunication between the components. The bridge routes both planes independantly. For ease of implementation it was selected for the bridge to act as a control plane TCP server, but it is a data plane **TCP client**, this must be taken into account when creating the network firewall and routing rules.
Pictures showing the network topology can be found in the [docs/network_topology.pdf](./docs/network_topology.pdf).

## How to access logs
### C++ logging 
C++ based components utilize the cpplogging library for fast binary logging. The cpplogging library also supports text based logging to the output. Applications are designed to be switchable between both types at compilation time (Change a cmakelist parameter in each component)
**output text based logging can cuase up to 200 us of latency per log message** Binary logging is recommended for anything except direct debuging as it causes less than 5 us of latency per log message.
#### How to access C++ binary logs
using the **BinLogDecoder.py** script located at the same directory the binary files apear is recomended. Since the library uses the fmt formating library the script uses known format specifiers to decode the binary logs. If some format specifiers are not recognized, they will be ignored and the script should be modified accordingly.
Please note that there are multiple BinlogDecoder.py scripts in this repository dedicated to different components. Make sure to change them all when updating
### Python logging
Python based components utilize the built-in logging library for logging. The logging library supports different log levels (DEBUG, INFO, WARNING, ERROR, CRITICAL) and can be configured to output logs in different formats. It is recommended to use the logging library's built-in features for log management.

## More detailed documentation for all components *WIP*. 
There are many .md files curently present in all components folders. THESE SHOULD NOT BE CONSIDERED DOCUMENTATION. Only files listed bellow can be used for reference. The other files were mainly used to guide asisted coding tools like github copilot and are not meant for human reading.
### list of reference documentation and user guides
- VHC and MEC - [ros/src/ros_ws/src/modular_gateway_sender/docs/user_guide_MGW.md](./ros/src/ros_ws/src/modular_gateway_sender/docs/user_guide_MGW.md)
- Bridge - [bridge/docs/user_guide_BRIDGE.md](./bridge/docs/user_guide_BRIDGE.md)
- Offloading Manager - [OffloadingManager/docs/user_guide_OM.md](./OffloadingManager/docs/user_guide_OM.md)
- Discovery Service - [DiscoveryService/docs/user_guide_DISC.md](./DiscoveryService/docs/user_guide_DISC.md)
- Testing Component - [ros/src/ros_ws/src/offloading_latency_test_loopback/docs/user_guide_TEST.md](./ros/src/ros_ws/src/offloading_latency_test_loopback/docs/user_guide_TEST.md)
- **protocol details** - [ros/src/ros_ws/src/modular_gateway_sender/docs/protocol_details.md](./ros/src/ros_ws/src/modular_gateway_sender/docs/protocol_details.md)
- **process flow diagrams** [ros/src/ros_ws/src/modular_gateway_sender/docs/process_flow_diagrams.md](./ros/src/ros_ws/src/modular_gateway_sender/docs/process_flow_diagrams.md)
- **p2p process flow diagrams** [ros/src/ros_ws/src/modular_gateway_sender/docs/p2p_process_flow_diagrams.md](./ros/src/ros_ws/src/modular_gateway_sender/docs/p2p_process_flow_diagrams.md)
- **network topology pictures** [docs/network_topology.pdf](./docs/network_topology.pdf).
- **NO OTHER GUIDES AND REFERENCES ARE CURRENTLY AVAILABLE.** 

## Requirements
- docker
- docker compose

## How to run
The system includes fail-broken mechanisms to prevent unintended behavior. One of these is in DISC. It wont allow any component to connect before the OM has registered, then it will allow the bridge to connect, and finally other components can be registered.

**HOWEVER** to prevent the issue of incorect global configuration, the DISC service will shut down if the OM disconects. Which means that the DISC must be restarted for every experiment. Also currently there is no mechanism to auto-asign the component ID, its recomended to restart the DISC even if the OM has not been disconected when changing experiments.

### Networked Mode
Order of launching the components:
1. Discovery Service
2. Offloading Manager
3. Bridge
4. **SYSTEM READY** other components can be launched.

### P2P Modes (p2p / p2p_ds)
These modes bypass the OM and Bridge, allowing direct connection between VHC and MEC using a local task configuration.
* **p2p (Pure P2P):** VHC and MEC connect directly using static IP addresses. Launch `p2p_mec_launch.py` and `p2p_vhc_launch.py`. No Discovery Service required.
* **p2p_ds (Discovery-Assisted P2P):** Uses the Discovery Service for matchmaking, but no OM/Bridge is required.
  1. Discovery Service (must be launched with the `--p2p` flag)
  2. `p2p_mec_launch.py` and `p2p_vhc_launch.py` (any order)

TBD

## YML overide files
To make launching the system easier, yml overide files are provided. bellow is a list of the overide files and their intended use

to run the overide options use the following command:
```
docker compose -f docker-compose.yml -f <overide file> up <container names (optional)> -d
```

- **[docker-compose.devmode.yml](./docker-compose.devmode.yml)** - this file disables the autostart script off all components, allowing for development containers to be launched.

- **[docker-compose.macvlan.override.yml](./docker-compose.macvlan.override.yml)** - this file launches the DISC, OM and Bridge components on a macvlan network. With dedicated IP addresses for each component. This allows those 3 components to be launched on a single computer with a single network interface. **MACVLAN setup required on the host computer; details are in the file comments**

- **[docker-compose.mec.yml](./docker-compose.mec.yml)** - this file launches only the MEC component using an autostart script. It is useful for runing the MEC container on a dedicated computer.

- **[docker-compose.vhc.yml](./docker-compose.vhc.yml)** - this file launches only the VHC component using an autostart script. It is useful for runing the VHC container on a dedicated computer.

## APPENDIX A - Discovery Service IP/Port Change Locations

Change the Discovery Service IP (and if needed the port) in ALL of the following exact locations:

1. Offloading Manager config: [OffloadingManager/algorithm.py](OffloadingManager/algorithm.py) -> DISCOVERY_SERVICE_HOST = os.getenv("DISCOVERY_SERVICE_HOST", "192.168.50.114")
2. Bridge Control Plane: [bridge/src/BridgeControlPlane.hpp](bridge/src/BridgeControlPlane.hpp) -> const std::string DISCOVERY_SERVICE_HOST = "192.168.50.114";
3. ROS2 VHC launch: [ros/src/ros_ws/src/offloading_latency_test_loopback/launch/vch_launch.py](ros/src/ros_ws/src/offloading_latency_test_loopback/launch/vch_launch.py) -> DeclareLaunchArgument('discovery_host', default_value='192.168.50.114', ...)
4. ROS2 MEC launch: [ros/src/ros_ws/src/offloading_latency_test_loopback/launch/mec_launch.py](ros/src/ros_ws/src/offloading_latency_test_loopback/launch/mec_launch.py) -> DeclareLaunchArgument('discovery_host', default_value='192.168.50.114', ...)
5. Docker macvlan override: [docker-compose.macvlan.override.yml](docker-compose.macvlan.override.yml) -> discovery_service: ipv4_address: 192.168.50.114
6. Discovery Service port (only if changing port): [DiscoveryService/CMakeLists.txt](DiscoveryService/CMakeLists.txt) -> DISCOVERY_SERVICE_PORT=9090

## APPENDIX B - NAMING CONVENTION

This section defines terminology used consistently across all components of the offloading system. For component-specific implementation details, refer to individual component user guides.

### System Components
* **component** – Any participating service in the offloading system (VHC, MEC, OM, Bridge, DiscoveryService).
* **component_id** – Unique identifier in format `group_id:id_in_group` (e.g., `60:5` or `15:10`). Both parts are 8-bit integers (0-255).
* **VHC** – Vehicle Host Component. Physical or simulated vehicle system that requests task offloading. Runs ROS2 environment and MGW software. Component type: "V".
* **MEC** – Mirror Execution Component. Virtual copy of the VHC running on a Mobile Edge Computing server. Processes offloaded tasks. Runs same ROS2 environment as VHC. Component type: "M".
* **OM** – Offloading Manager. Central decision-making service that approves or denies offloading requests based on resource availability and allocation algorithms. Written in Python. Component ID: `1:1`.
* **Bridge / BridgeCP / BridgeDP** – Central routing hub with two planes: Control Plane (BridgeCP) handles session management via JSON messages; Data Plane (BridgeDP) forwards binary ROS2 data between VHC and MEC. Written in C++. Component ID: `15:10`.
* **DiscoveryService / DISC** – Service registration and discovery system. Only component with fixed IP address. Distributes global configuration and provides component addresses to all participants. Port: 9090.
* **MGW** – Modular Gateway. Software layer on VHC/MEC that interfaces between ROS2 topics and the offloading system's data/control planes.

### Network Architecture
* **CP** – Control Plane. Communication channel for session negotiation, keepalives, and management messages. Uses JSON over TCP.
* **DP** – Data Plane. Communication channel for actual ROS2 message transmission. Uses binary protocol over TCP with custom framing.
* **MGWCP** – Modular Gateway Control Plane protocol. JSON-based protocol for session management between MGW (VHC/MEC) and Bridge.
* **MGWDP** – Modular Gateway Data Plane protocol. Binary protocol for ROS2 message encapsulation and transmission.

### Offloading Concepts
* **task** – Offloadable computational unit defined in global configuration. Specifies which ROS2 message types are inputs/outputs. Identified by `task_id` and `task_name`.
* **request** – Single offloading request initiated by VHC. Identified by unique `request_id` generated by VHC and the VHC's `component_id`.
* **session** – Active offloading session between specific VHC and MEC for one task. Maintained by periodic keepalives. Lifetime: from SESSION_APPROVED until termination or timeout.
* **global configuration JSON** – Authoritative catalog of available tasks and system parameters. Created by OM, distributed by DiscoveryService to all components at registration.
* **vhc_data** – Optional application-specific context data (string) included in offloading requests for algorithm decision-making.

### Protocol Elements  
* **message_code** – Numeric identifier for control plane message types (100-199: MGW→Bridge, 200-299: Bridge→MGW, 300-399: Bridge→OM, 900-999: Control).
* **message_type** – Human-readable string corresponding to message_code (e.g., "OFFLOAD_REQUEST", "SESSION_APPROVED").
* **MessageType** – Numeric enumeration for data plane message types identifying ROS2 message format in binary headers (e.g., 201=STRING_TEST_INPUT).
* **sequence_number** – Monotonically increasing counter per TCP connection for message ordering and ACK matching. Starts at 1.
* **ACK** – Acknowledgement message confirming receipt. Used for hop-by-hop reliability on MGW-Bridge link (5s timeout, 3 retries).
* **keepalive** – Periodic heartbeat messages maintaining connection/session liveness. Two types: Discovery keepalive (5s interval) and session keepalive (15s interval, 20s timeout).

### Routing & Session Management
* **routing_table** – Bridge's internal data structure mapping `(source_id, message_type)` tuples to destination transport handlers for message forwarding.
* **transport_handler** – Bridge component managing TCP connections and message queuing for specific destinations (VHC/MEC gateways).
* **handler** – MGW component that bridges ROS2 topics to the data plane. Subscribes to topics, serializes messages, and publishes received data.
* **handler mode** – Direction configuration: SUBSCRIBER_ONLY (sends data), PUBLISHER_ONLY (receives data), or BOTH (bidirectional).

### Configuration & Logging
* **group_id** – First part of component_id. Identifies component category (e.g., 1=system, 10=VHC-manual, 20=VHC-auto, 50=MEC).
* **id_in_group** – Second part of component_id. Unique identifier within a group_id category.
* **binary log** – High-performance logging format (CppLogging library) requiring decoder. Used by C++ components (Bridge, MGW). ~5μs latency per message.
* **text log** – Human-readable logging format. Used by Python components (OM) or C++ debug builds. ~200μs latency per message.
* **BinLogDecoder.py** – Python tool for converting binary logs to human-readable text format. Located in component directories.

### Common Message Types (Control Plane)
* **OFFLOAD_REQUEST (100)** – VHC initiates offloading for specific task.
* **SESSION_APPROVED (200)** – OM approves request and assigns MEC.
* **SESSION_DENIED (201)** – OM denies request with reason code.
* **SESSION_KEEPALIVE (101)** – VHC maintains active session.
* **SESSION_TERMINATE_REQUEST (102)** – VHC requests graceful session end.
* **DP_INFO (103)** – MGW provides data plane connection details to Bridge (first message after TCP connect).
* **DP_CONNECTION_CONFIRMED (202)** – Bridge confirms data plane connection established.
* **BRIDGE_DP_FAILURE (301)** – Bridge reports data plane connection failure to OM.

### Reason Codes (for SESSION_DENIED)
* **4001 - INSUFFICIENT_RESOURCES** – No available MEC capacity for request.
* **4002 - TASK_NOT_FOUND** – Requested task_id not in global configuration.
* **4003 - SESSION_TIMEOUT** – Session expired due to missing keepalives (>20s).
* **4004 - INVALID_REQUEST** – Malformed request message.
* **4005 - FINISHED_SESSION** – Graceful termination requested by component.