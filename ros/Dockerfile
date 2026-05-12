# Použijte ROS 2 Jazzy jako základní image
FROM ros:jazzy
# ROS2 dockerfile

RUN export ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F\" '{print $4}') ;\
    curl -L -s -o /tmp/ros2-apt-source.deb "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo $VERSION_CODENAME)_all.deb" \
    && apt-get update \
    && apt-get install /tmp/ros2-apt-source.deb \
    && rm -f /tmp/ros2-apt-source.deb

# Instalace balíčků pro ROS 2 a gz_bridge
RUN apt update && apt install -y \
ros-jazzy-demo-nodes-cpp \
ros-jazzy-rviz2 \
ros-jazzy-teleop-twist-keyboard \
ros-jazzy-slam-toolbox \
ros-jazzy-joint-state-publisher \
ros-jazzy-joint-state-publisher-gui \
ros-jazzy-xacro \
nano \ 
iputils-ping \
iproute2 \
python3-pip \
binutils-dev \
uuid-dev 

RUN apt install -y tmux \
ros-jazzy-robot-localization \
ros-jazzy-navigation2 \
ros-jazzy-nav2-bringup \
#ros-jazzy-rmw-cyclonedx-cpp \ # MUST BE FIXED - THROWS DOCKER BUILD ERROR
ros-jazzy-pcl-conversions \
ros-jazzy-pcl-msgs \
python3-scipy \
python3-serial 

# Install gil (used by CppCommon and CppLogging)
RUN python3 -m pip config set global.break-system-packages true
RUN pip3 install gil

# --- Setup External Libraries (CppCommon, CppLogging, fmt, moodycamel) ---
# Create a directory to hold the cloned sources
ARG EXTERNAL_LIBS_DIR=/home/ubuntu/external_libs
RUN mkdir -p ${EXTERNAL_LIBS_DIR}
WORKDIR ${EXTERNAL_LIBS_DIR}

# --- Clone CppCommon ---
RUN git clone https://github.com/chronoxor/CppCommon.git CppCommon && \
    cd CppCommon && \
    git checkout 1.0.5.0 && \
    sed -i 's/fmt.git master/fmt.git main/g' .gitlinks && \
    gil update && \
    cd build && \
    CXXFLAGS="-Wno-error=deprecated-declarations -fPIC" CFLAGS="-fPIC" ./unix.sh

# --- Clone CppLogging ---
# WORKDIR is still ${EXTERNAL_LIBS_DIR}
RUN git clone https://github.com/chronoxor/CppLogging.git CppLogging && \
    cd CppLogging && \
    git checkout 1.0.5.0 && \
    (gil update || true) && \
    sed -i 's/fmt.git master/fmt.git main/g' modules/CppCommon/.gitlinks && \
    gil update && \
    cd build && \
    CXXFLAGS="-Wno-error=deprecated-declarations -fPIC" CFLAGS="-fPIC" ./unix.sh

# --- Clone and Build fmt ---
# WORKDIR is still ${EXTERNAL_LIBS_DIR}
RUN git clone https://github.com/fmtlib/fmt.git fmt && \
    cd fmt && \
    git checkout 12.1.0 && \
    mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX=/usr/local -DFMT_TEST=OFF && \
    make -j$(nproc) && \
    make install
# fmt is now installed to /usr/local

# --- Clone and Build moodycamel::readerwriterqueue ---
RUN git clone https://github.com/cameron314/readerwriterqueue.git readerwriterqueue && \
    cd readerwriterqueue && \
    mkdir -p build && cd build && \
    cmake .. && \
    make -j$(nproc) && \
    make install

# --- Permanent Library Path Configuration ---
# Method 1: Add /usr/local/lib to system-wide ld.so configuration
RUN echo "/usr/local/lib" >> /etc/ld.so.conf.d/usr-local.conf && \
    ldconfig

# Method 2: Set LD_LIBRARY_PATH in environment for all users
RUN echo 'export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"' >> /etc/environment

# Method 3: Add to system-wide bash configuration
RUN echo 'export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"' >> /etc/bash.bashrc

COPY ./src/ros_ws/src /home/ubuntu/ros_ws/src/
COPY .github /home/ubuntu/ros_ws/.github

# Nastavení prostředí pro ROS 2
RUN echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc

# Add library path to ROS 2 setup for current user
RUN echo 'export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"' >> ~/.bashrc

# # Inicializace rosdep
WORKDIR /home/ubuntu/ros_ws
RUN rm -rf build/ install/ log/
RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash"

# Set environment variable for this Docker layer and all subsequent ones
ENV LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"

##building all packages
#RUN /bin/bash -c "source /opt/ros/jazzy/setup.bash && colcon build"
#RUN echo "source /home/ubuntu/ros_ws/install/setup.bash" >> ~/.bashrc
#RUN /bin/bash -c "source /home/ubuntu/ros_ws/install/setup.bash"

# Výchozí příkaz pro spuštění bash
CMD ["bash"]