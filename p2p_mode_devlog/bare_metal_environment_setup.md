# Bare-Metal Environment Setup Guide
# Modular Gateway Sender — ROS 2 Jazzy

This guide replicates the Docker build environment on a bare-metal machine.
Follow it once on any new machine before building the ROS 2 workspace.

> [!IMPORTANT]
> The `CMakeLists.txt` hardcodes the external library paths to `/home/ubuntu/external_libs/`.
> You **must** use this exact path, or edit the three `set(...)` lines at the top of `CMakeLists.txt` to match your chosen location before building.

---

## 0. Prerequisites

- Ubuntu 24.04 LTS (Noble Numbat)
- ROS 2 Jazzy installed ([official guide](https://docs.ros.org/en/jazzy/Installation.html))
- Internet access during setup

---

## 1. Install System Dependencies

```bash
# Add the ROS 2 apt source (skip if already done during ROS2 install)
ROS_APT_SOURCE_VERSION=$(curl -s https://api.github.com/repos/ros-infrastructure/ros-apt-source/releases/latest | grep -F "tag_name" | awk -F\" '{print $4}')
curl -L -s -o /tmp/ros2-apt-source.deb \
  "https://github.com/ros-infrastructure/ros-apt-source/releases/download/${ROS_APT_SOURCE_VERSION}/ros2-apt-source_${ROS_APT_SOURCE_VERSION}.$(. /etc/os-release && echo $VERSION_CODENAME)_all.deb"
sudo apt-get update
sudo apt-get install /tmp/ros2-apt-source.deb

# Core build dependencies
sudo apt update && sudo apt install -y \
    nano \
    iputils-ping \
    iproute2 \
    python3-pip \
    binutils-dev \
    uuid-dev \
    ros-jazzy-sensor-msgs
```

---

## 2. Install Python Build Tools

The `gil` tool is required to resolve the sub-module dependencies of `CppCommon` and `CppLogging`.

```bash
pip install gil --break-system-packages
```

> [!IMPORTANT]
> `pip` installs user-level packages to `~/.local/bin`, which is **not** in `PATH` by default.
> You must add it before the build steps, and to your shell profile so it persists.

```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verify gil is found:
which gil   # expected: /home/pi/.local/bin/gil
```

---

## 3. Build External C++ Libraries

> [!IMPORTANT]
> All libraries **must** be cloned into `/home/ubuntu/external_libs/` due to the hardcoded paths in `CMakeLists.txt`.
> Because your username is `pi` (not `ubuntu`), create that directory and take ownership:

```bash
sudo mkdir -p /home/ubuntu/external_libs
sudo chown -R pi:pi /home/ubuntu/external_libs
cd /home/ubuntu/external_libs
```

### 3.1 CppCommon

> [!NOTE]
> `unix.sh` invokes cmake internally and sets its own `-Werror` flags, ignoring the `CXXFLAGS` environment variable. On **ARM64** (Raspberry Pi) this causes a hard error in the AVL tree benchmarks. The fix is to invoke cmake directly so compiler flags are reliably applied.

```bash
git clone https://github.com/chronoxor/CppCommon.git CppCommon && \
cd CppCommon && \
git checkout 1.0.5.0 && \
sed -i 's/fmt.git master/fmt.git main/g' .gitlinks && \
gil update && \
mkdir -p build && cd build && \
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-Wno-error=deprecated-declarations -Wno-error=switch-outside-range -fPIC" \
    -DCMAKE_C_FLAGS="-fPIC" && \
make -j$(nproc)
cd /home/ubuntu/external_libs
```

### 3.2 CppLogging

```bash
git clone https://github.com/chronoxor/CppLogging.git CppLogging && \
cd CppLogging && \
git checkout 1.0.5.0 && \
(gil update || true) && \
sed -i 's/fmt.git master/fmt.git main/g' modules/CppCommon/.gitlinks && \
gil update && \
mkdir -p build && cd build && \
cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-Wno-error=deprecated-declarations -Wno-error=switch-outside-range -fPIC" \
    -DCMAKE_C_FLAGS="-fPIC" && \
make -j$(nproc)
cd /home/ubuntu/external_libs
```

### 3.3 fmt

```bash
git clone https://github.com/fmtlib/fmt.git fmt
cd fmt
git checkout 12.1.0
mkdir -p build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DFMT_TEST=OFF
make -j$(nproc)
sudo make install
cd /home/ubuntu/external_libs
```

### 3.4 moodycamel readerwriterqueue

```bash
git clone https://github.com/cameron314/readerwriterqueue.git readerwriterqueue
cd readerwriterqueue
mkdir -p build && cd build
cmake ..
make -j$(nproc)
sudo make install
cd /home/ubuntu/external_libs
```

---

## 4. Configure the Dynamic Linker

Make the installed libraries discoverable at runtime.

```bash
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/usr-local.conf
sudo ldconfig

# Add to your shell profile so it persists across sessions
echo 'export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"' >> ~/.bashrc
source ~/.bashrc
```

---

## 5. Build and Install the DiscoveryProtocol Library

`DiscoveryProtocol` is a custom C++ library included in this repository. It must be built and installed to `/usr/local` before the ROS 2 workspace can find it via `find_package(DiscoveryProtocol)`.

```bash
cd <repo_root>/cpplib/discovery_protocol
# Example: cd ~/RobotArmProject/ROS2_modular_edge_offloading/cpplib/discovery_protocol
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
make -j$(nproc)
sudo make install
sudo ldconfig
```

---

## 6. Build the ROS 2 Workspace

> [!WARNING]
> You **must** source the base ROS environment before running colcon. Without it, the build fails with `No module named 'ament_package'` and `ros2: command not found`. Set up `~/.bashrc` first so every new terminal is ready automatically.

**Step 1 — Add both sources to `~/.bashrc` permanently:**
```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
echo "source <your_workspace_root>/install/setup.bash" >> ~/.bashrc
# Example:
# echo "source ~/RobotArmProject/ROS2_modular_edge_offloading/ros/src/ros_ws/install/setup.bash" >> ~/.bashrc
source ~/.bashrc
```

**Step 2 — Build:**
```bash
# Navigate to your ROS 2 workspace root (the directory that contains the src/ folder)
# Example: cd ~/RobotArmProject/ROS2_modular_edge_offloading/ros/src/ros_ws
cd <your_workspace_root>
source /opt/ros/jazzy/setup.bash   # required before colcon
colcon build --packages-select modular_gateway_sender
source install/setup.bash
```

---

## 7. Verify the Installation

> [!IMPORTANT]
> `source ~/.bashrc` only loads the global ROS environment. You must also source the **workspace install overlay** for `ros2` to see the package.

```bash
source install/setup.bash
ros2 pkg list | grep modular_gateway_sender
```

Expected output:
```
modular_gateway_sender
```

Check the available launch arguments (note: `--show-args` goes **after** the launch file name):
```bash
ros2 launch modular_gateway_sender robotic_arm_vhc_launch.py --show-args
```

---

## 7. Next Steps

Refer to [robotic_arm_launch_procedure.md](./robotic_arm_launch_procedure.md) for how to start the VHC node for the robotic arm offloading use case.
