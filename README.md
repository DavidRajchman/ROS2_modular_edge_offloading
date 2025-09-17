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
- [offloading-manager](./bridge/Dockerfile) image runs the offloading manager for the offloading system. It is responsible for coordinating the offloading process and making decisions about which tasks to offload. It contains the [global configuration](./OffloadingManager/config.json) for the offloading system. Which list all configurable experiments. It also contains the [algorithm](./OffloadingManager/algorithm.py) which is the only file that should be edited during research. It contains OM configuration as well as the actual offloading algorithm (single persistent thread). Curently an autoaproove placeholder is present.
- [discovery](./bridge/Dockerfile) image runs the discovery service for the offloading system. It is responsible for service registration and discovery. It also distributes the global configuration to other components.
- [bridge](./bridge/Dockerfile) image acts as a mobile edge based data router. It is responsible for routing data between the VHC and MEC containers.
- [ros2_vhc](./ros/Dockerfile) image runs the ROS2 environment for the autonomous vehicle.
- [ros2_mec](./ros/Dockerfile) image runs the ROS2 environment for the virtual copy of the VHC. It waits for offloading requests to be accepted. Utilizes the same dockerfile as VHC

## Network architecture
The entire system is connected through the bridge component (except for the discovery service and its direct connection to all components). There are 2 separeted parts of every aplication on diferent sockets - data plane and control plane. Data plane is solely reserved for data transfer, while control plane is used for signaling comunication between the components. The bridge routes both planes independantly. For ease of implementation it was selected for the bridge to act as a control plane TCP server, but it is a data plane **TCP client**, this must be taken into account when creating the network firewall and routing rules.
Pictures showing the network topology can be found in the [docs/network_topology.pdf](docs/network_topology.pdf).

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
- VHC and MEC - [ros/src/ros_ws/src/modular_gateway_sender/docs/user_guide.md](./ros/src/ros_ws/src/modular_gateway_sender/docs/user_guide.md)
- **NO OTHER GUIDES AND REFERENCES ARE CURRENTLY AVAILABLE.** 

## Requirements
- docker
- docker compose

## How to run
The system includes fail-broken mechanisms to prevent unintended behavior. One of these is in DISC. It wont allow any component to connect before the OM has registered, then it will allow the bridge to connect, and finally other components can be registered.

**HOWEVER** to prevent the issue of incorect global configuration, the DISC service will shut down if the OM disconects. Which means that the DISC must be restarted for every experiment. Also currently there is no mechanism to auto-asign the component ID, its recomended to restart the DISC even if the OM has not been disconected when changing experiments.

Order of launching the components:
1. Discovery Service
2. Offloading Manager
3. Bridge
4. **SYSTEM READY** other components can be launched.

TBD

## YML overide files
To make launching the system easier, yml overide files are provided. bellow is a list of the overide files and their intended use

to run the overide options use the following command:
```
docker compose -f docker-compose.yml -f <overide file> up <container names (optional)> -d
```

- [docker-compose.devmode.yml](./docker-compose.devmode.yml) - this file disables the autostart script off all components, allowing for development containers to be launched.

- [docker-compose.macvlan.yml](./docker-compose.macvlan.override.yml) - this file launches the DISC, OM and Bridge components on a macvlan network. With dedicated IP addresses for each component. This allows those 3 components to be launched on a single computer with a single network interface. **MACVLAN setup required on the host computer; details are in the .mcvlan file comments**

- [docker-compose.mec.yml](./docker-compose.mec.override.yml) - this file launches only the MEC component using an autostart script. It is useful for runing the MEC container on a dedicated computer.

- [docker-compose.vhc.yml](./docker-compose.vhc.override.yml) - this file launches only the VHC component using an autostart script. It is useful for runing the VHC container on a dedicated computer.

## APPENDIX A - Discovery Service IP/Port Change Locations

Change the Discovery Service IP (and if needed the port) in ALL of the following exact locations:

1. Offloading Manager config: [OffloadingManager/algorithm.py](OffloadingManager/algorithm.py) -> DISCOVERY_SERVICE_HOST = os.getenv("DISCOVERY_SERVICE_HOST", "192.168.50.114")
2. Bridge Control Plane: [bridge/src/BridgeControlPlane.hpp](bridge/src/BridgeControlPlane.hpp) -> const std::string DISCOVERY_SERVICE_HOST = "192.168.50.114";
3. ROS2 VHC launch: [ros/src/ros_ws/src/offloading_latency_test_loopback/launch/vch_launch.py](ros/src/ros_ws/src/offloading_latency_test_loopback/launch/vch_launch.py) -> DeclareLaunchArgument('discovery_host', default_value='192.168.50.114', ...)
4. ROS2 MEC launch: [ros/src/ros_ws/src/offloading_latency_test_loopback/launch/mec_launch.py](ros/src/ros_ws/src/offloading_latency_test_loopback/launch/mec_launch.py) -> DeclareLaunchArgument('discovery_host', default_value='192.168.50.114', ...)
5. Docker macvlan override: [docker-compose.macvlan.override.yml](docker-compose.macvlan.override.yml) -> discovery_service: ipv4_address: 192.168.50.114
6. Discovery Service port (only if changing port): [DiscoveryService/CMakeLists.txt](DiscoveryService/CMakeLists.txt) -> DISCOVERY_SERVICE_PORT=9090

## APPENDIX B - NAMING CONVENTION *WIP*
Core terms actually used in the codebase (see referenced files):
* **component** – Any participating runtime (VHC, MEC, OM, Bridge, DiscoveryService).
* **component_id** – Concatenation `group_id:id_in_group` (e.g. `60:5`) built in `gateway_controller.cpp` from parameters `identity.group_id` & `identity.id_in_group`.
* **VHC** – Vehicle host component (identity.component_type="V"). Requests offloading via service `request_offloading`.
* **MEC** – Mirror execution component (identity.component_type="M"). Receives unsolicited SESSION_APPROVED and processes data.
* **OM** – Offloading Manager (external, not implemented here) that decides approvals.
* **Bridge / BridgeCP / BridgeDP** – Bridge Control Plane (JSON session/control messages) & Data Plane (binary framed payload). The gateway connects CP via `BridgeCpClient` and DP via `TcpServerTransport`.
* **DiscoveryService** – Entry point supplying `RegistrationResponse` with `configJson` consumed in `GatewayController::on_discovery_success`.
* **DP** – Data Plane (binary framed messages; see `message_header.hpp`).
* **CP** – Control Plane (session negotiation & keepalives; see `bridge_cp_client.cpp`).
* **MGW** – Modular Gateway node (`gateway_controller` executable for VHC; `mec_gateway` for MEC) orchestrating handlers + transports.
* **task** – Offloadable unit defined in global config JSON (fields: `task_id`, `task_name`, `input_message_types`, `output_message_types`). Parsed into internal TaskDetails in `gateway_controller.cpp`.
* **request** – OFFLOAD_REQUEST initiated by VHC (see `BridgeCpClient::send_offload_request`). Identified by `request_id` (local monotonic counter) + component_id.
* **session** – Active offloaded task lifecycle between VHC & MEC. Internally tracked in `active_sessions_` keyed by `request_id` (no separate session ID on the wire).
* **MessageType** – Numeric enumeration (`message_header.hpp`) used for both global config and binary header type byte.
* **handler** – Concrete subclass of `MessageHandlerBase` bridging a ROS 2 topic to the MGW DP (e.g. `StringTestInputHandler`).
* **handler mode** – One of SUBSCRIBER_ONLY / PUBLISHER_ONLY / BOTH (see `message_handler_base.hpp`, configured per role in `on_session_approved`).
* **task database** – In-memory map `<task_id, TaskDetails>` populated once from `configJson` (see `load_global_config_from_json`).
* **global configuration JSON** – Distributed once via Discovery; authoritative catalog of tasks.
* **binary log** – Persistent logging output (CppLogging format) including per-source file prefixes; used for diagnostics and research analysis.
* **keepalive** – CP ping/pong for session liveness (Discovery keepalive) & session-level keepalives (Bridge CP).
* **ACK** – Control-plane acknowledgement for reliable message sequences (managed in `BridgeCpClient`).