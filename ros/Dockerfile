# Použijte ROS 2 Jazzy jako základní image
FROM ros:jazzy

# Nastavte pracovní adresář
COPY ./src /home/ubuntu/

# Instalace balíčků pro ROS 2 a gz_bridge
RUN apt update && apt install -y \
ros-jazzy-demo-nodes-cpp \
ros-jazzy-rviz2 \
ros-jazzy-teleop-twist-keyboard \
ros-jazzy-slam-toolbox \
nano

RUN apt install -y tmux \
ros-jazzy-robot-localization \
ros-jazzy-navigation2 \
ros-jazzy-nav2-bringup \
ros-jazzy-rmw-cyclonedds-cpp \
ros-jazzy-pcl-conversions \
ros-jazzy-pcl-msgs \
python3-scipy



# Nastavení prostředí pro ROS 2
RUN echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc

# # Inicializace rosdep
WORKDIR /home/ubuntu/ros_ws
RUN rm -rf build/ install/ log/
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash"
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && colcon build"
RUN echo "source /home/ubuntu/ros_ws/install/setup.bash" >> ~/.bashrc
RUN /bin/bash -c "source /home/ubuntu/ros_ws/install/setup.bash"

# Výchozí příkaz pro spuštění bash
CMD ["bash"]


