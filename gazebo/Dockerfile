# Použijte ROS 2 Jazzy jako základní image
FROM ros:jazzy

# Nastavte pracovní adresář
# WORKDIR /root
COPY ./src /home/ubuntu/
# Instalace balíčků pro ROS 2 a gz_bridge
RUN apt update && apt install -y \
ros-jazzy-demo-nodes-cpp \
ros-jazzy-ros-gz-bridge \
ros-jazzy-ros-gz-sim \
ros-jazzy-ament-cmake \
nano \
tmux


RUN apt-get install lsb-release gnupg
RUN apt install curl

RUN curl https://packages.osrfoundation.org/gazebo.gpg --output /usr/share/keyrings/pkgs-osrf-archive-keyring.gpg &&\
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/pkgs-osrf-archive-keyring.gpg] http://packages.osrfoundation.org/gazebo/ubuntu-stable $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/gazebo-stable.list > /dev/null &&\
apt-get update

RUN apt-get install -y gz-harmonic


# Nastavení prostředí pro ROS 2
RUN echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
# RUN echo "export ROS_DISTRO=jazzy" >> ~/.bashrc

# # Inicializace rosdep
WORKDIR /home/ubuntu/ros_ws
RUN rm -rf build/ install/ log/
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && colcon build --symlink-install"
RUN echo "source /home/ubuntu/ros_ws/install/setup.bash" >> ~/.bashrc
RUN /bin/bash -c "source /home/ubuntu/ros_ws/install/setup.bash"

# Výchozí příkaz pro spuštění bash
CMD ["bash"]


