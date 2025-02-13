# Použijte ROS 2 Jazzy jako základní image
FROM ros:jazzy

# Nastavte pracovní adresář
# WORKDIR /root
COPY ./src /home/ubuntu/
# Instalace balíčků pro ROS 2 a gz_bridge
RUN apt update && apt install -y \
ros-jazzy-demo-nodes-cpp \
ros-jazzy-rviz2 \
nano



# Nastavení prostředí pro ROS 2
RUN echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
# RUN echo "export ROS_DISTRO=jazzy" >> ~/.bashrc

# # Inicializace rosdep
WORKDIR /home/ubuntu/ros_ws
RUN rm -rf build/ install/ log/
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && colcon build"
RUN echo "source /home/ubuntu/ros_ws/install/setup.bash" >> ~/.bashrc
RUN /bin/bash -c "source /home/ubuntu/ros_ws/install/setup.bash"

# Výchozí příkaz pro spuštění bash
CMD ["bash"]


