# Použijte ROS 2 Jazzy jako základní image
FROM ros:jazzy


RUN export ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F\" '{print $4}') ;\
    curl -L -s -o /tmp/ros2-apt-source.deb "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo $VERSION_CODENAME)_all.deb" \
    && apt-get update \
    && apt-get install /tmp/ros2-apt-source.deb \
    && rm -f /tmp/ros2-apt-source.deb


# Nastavte pracovní adresář
COPY ./src /home/ubuntu/

# Instalace balíčků pro ROS 2 a gz_bridge
RUN apt update && apt install -y \
ros-jazzy-demo-nodes-cpp \
ros-jazzy-rviz2 \
ros-jazzy-teleop-twist-keyboard \
ros-jazzy-slam-toolbox \
nano \ 
iputils-ping \
iproute2 


RUN apt install -y tmux \
ros-jazzy-robot-localization \
ros-jazzy-navigation2 \
ros-jazzy-nav2-bringup \
ros-jazzy-rmw-cyclonedds-cpp \
ros-jazzy-pcl-conversions \
ros-jazzy-pcl-msgs \
python3-scipy \
python3-serial \
libpaho-mqttpp-dev \
libpaho-mqtt-dev



# Nastavení prostředí pro ROS 2
RUN echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc

# # Inicializace rosdep
WORKDIR /home/ubuntu/ros_ws
RUN rm -rf build/ install/ log/
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash"

##building all packages
#RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && colcon build"
#RUN echo "source /home/ubuntu/ros_ws/install/setup.bash" >> ~/.bashrc
#RUN /bin/bash -c "source /home/ubuntu/ros_ws/install/setup.bash"

#Instalace paho.mqtt.cpp
#RUN cd /home/ubuntu/
#RUN mkdir cpp_libraries
#RUN git clone https://github.com/eclipse/paho.mqtt.cpp
#RUN cd paho.mqtt.cpp
#RUN git checkout v1.5.2
#RUN git submodule init
#RUN git submodule update
#RUN cmake -Bbuild -H. -DPAHO_WITH_MQTT_C=ON -DPAHO_BUILD_EXAMPLES=ON
#RUN sudo cmake --build build/ --target install

# Výchozí příkaz pro spuštění bash
CMD ["bash"]


