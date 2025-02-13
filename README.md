# Autonomous Driving ROS2


## Getting started

The respository includes 2 docker images. Both of them are started using [docker-compose.yml](./docker-compose.yml) file. 
- [gz-ros2](./gazebo/Dockerfile) image run the simulation using ackermann steering for autonomous vehicle model. Vehicle model containes Lidar, IMU and camera sensors. The communication with ROS2 environment ensure ros_gz_bridge package. Both entities are launched at the start of the container.
- [ros2](./ros/Dockerfile) image run the codes for autonomous vehicle driving. The image contains ROS2:jazzy environment with RVIZ2.

Both images enable GUI with the host system, you have to just enable it on your computer.

## Requirements
- docker
- docker compose

## How to run
```bash
# to enable GUI on your host comuter for local connection with user docker
xhost +local:docker 

# build docker compose
cd ~/autonomous-driving-ros2
docker compose build


# run both containers
cd ~/autonomous-driving-ros2
docker compose up -d  #in detached mode


# connect to the running container
docker compose exec -it <name_container> /bin/bash

# see logs of the running container
docker compose logs -f <name_container>

#names of the available containers
<name_container> = {gz-ros2, ros2}

#control simulation using teleop_twist_keyboard
ros2 run teleop_twist_keyboard teleop_twist_keyboard --ros-args --remap cmd_vel:=model/vehicle_blue/cmd_vel
```



