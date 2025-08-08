#!/bin/bash
set -euo pipefail

echo "=== MEC Autostart Script ==="

# Configurable paths (override via environment if needed)
CPPLIB_SRC_DIR="${CPPLIB_SRC_DIR:-/home/ubuntu/cpplib_src}"
ROS_WS="${ROS_WS:-/home/ubuntu/ros_ws}"
DISCOVERY_DIR="${DISCOVERY_DIR:-${CPPLIB_SRC_DIR}/discovery_protocol}"

BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"

echo "User: $(whoami)"
echo "Using CPPLIB_SRC_DIR=${CPPLIB_SRC_DIR}"
echo "Using ROS_WS=${ROS_WS}"
echo "Using DISCOVERY_DIR=${DISCOVERY_DIR}"

if [[ ! -d "${CPPLIB_SRC_DIR}" ]]; then
  echo "ERROR: CPPLIB_SRC_DIR '${CPPLIB_SRC_DIR}' does not exist." >&2
  exit 1
fi

if [[ ! -d "${DISCOVERY_DIR}" ]]; then
  echo "ERROR: discovery_protocol directory '${DISCOVERY_DIR}' not found." >&2
  exit 1
fi

echo "== Building discovery_protocol =="

cd "${DISCOVERY_DIR}"

if [[ ! -d build ]]; then
  echo "Creating build directory..."
  mkdir build
fi

cd build

if [[ ! -f CMakeCache.txt || "${FORCE_RECONFIGURE:-0}" == "1" ]]; then
  echo "Running cmake .. (BUILD_TYPE=${BUILD_TYPE})"
  cmake .. -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
else
  echo "CMake already configured (skip). Use FORCE_RECONFIGURE=1 to force."
fi

echo "Compiling discovery_protocol (jobs: ${JOBS})..."
make -j"${JOBS}"

echo "Installing discovery_protocol..."
make install

echo "== Returning to ROS workspace and building selected packages =="

cd "${ROS_WS}"

if [[ ! -d src ]]; then
  echo "ERROR: ROS workspace at '${ROS_WS}' missing 'src' directory." >&2
  exit 1
fi


if [[ "${CLEAN:-0}" == "1" ]]; then
  echo "Cleaning previous build/ install/ log/..."
  rm -rf build install log
fi

echo "Building modular_gateway_sender and offloading_latency_test_loopback..."
colcon build --packages-select modular_gateway_sender offloading_latency_test_loopback

echo "Sourcing workspace..."
source "${ROS_WS}/install/setup.bash"

echo "Launching MEC stack..."
exec ros2 launch offloading_latency_test_loopback mec_launch.py