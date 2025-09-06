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

If using multiple computers is needed for testing the offloading in a realworld scenario, the simplest solution is to use dedicated HW for those 3 restricted components. However an untested solution using macvlan has beed implemented and is able to launch (but there were isues with network routing, likely unrelated to the macvlan itself.) See `docker-compose.macvlan.override.yml` for more information on how to setup the macvlan 

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

## More detailed documentation for all components <WIP>. 
There are many .md files curently present in all components folders. THESE SHOULD NOT BE CONSIDERED DOCUMENTATION. Only files listed bellow can be used for reference. The other files were mainly used to guide asisted coding tools like github copilot and are not meant for human reading.
### list of reference documentation and user guides
- VHC and MEC - [ros/src/ros_ws/src/modular_gateway_sender/docs/user_guide.md](./ros/src/ros_ws/src/modular_gateway_sender/docs/user_guide.md)
- **NO OTHER GUIDES AND REFERENCES ARE CURRENTLY AVAILABLE.** 

## Requirements
- docker
- docker compose

## How to run
TBD 


## APPENDIX A - Discovery Service IP/Port Change Locations

Change the Discovery Service IP (and if needed the port) in ALL of the following exact locations:

1. Offloading Manager config: [OffloadingManager/algorithm.py](OffloadingManager/algorithm.py) -> DISCOVERY_SERVICE_HOST = os.getenv("DISCOVERY_SERVICE_HOST", "192.168.50.114")
2. Bridge Control Plane: [bridge/src/BridgeControlPlane.hpp](bridge/src/BridgeControlPlane.hpp) -> const std::string DISCOVERY_SERVICE_HOST = "192.168.50.114";
3. ROS2 VHC launch: [ros/src/ros_ws/src/offloading_latency_test_loopback/launch/vch_launch.py](ros/src/ros_ws/src/offloading_latency_test_loopback/launch/vch_launch.py) -> DeclareLaunchArgument('discovery_host', default_value='192.168.50.114', ...)
4. ROS2 MEC launch: [ros/src/ros_ws/src/offloading_latency_test_loopback/launch/mec_launch.py](ros/src/ros_ws/src/offloading_latency_test_loopback/launch/mec_launch.py) -> DeclareLaunchArgument('discovery_host', default_value='192.168.50.114', ...)
5. Docker macvlan override: [docker-compose.macvlan.override.yml](docker-compose.macvlan.override.yml) -> discovery_service: ipv4_address: 192.168.50.114
6. Discovery Service port (only if changing port): [DiscoveryService/CMakeLists.txt](DiscoveryService/CMakeLists.txt) -> DISCOVERY_SERVICE_PORT=9090
