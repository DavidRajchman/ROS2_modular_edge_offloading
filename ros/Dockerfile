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
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && colcon build"
RUN echo "source /home/ubuntu/ros_ws/install/setup.bash" >> ~/.bashrc
RUN /bin/bash -c "source /home/ubuntu/ros_ws/install/setup.bash"

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


