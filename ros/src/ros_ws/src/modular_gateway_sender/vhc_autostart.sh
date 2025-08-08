#!/bin/bash
set -euo pipefail

echo "=== VHC Autostart Script ==="

# Configurable paths (override via environment if needed)
CPPLIB_SRC_DIR="${CPPLIB_SRC_DIR:-/home/ubuntu/cpplib_src}"
ROS_WS="${ROS_WS:-/home/ubuntu/ros_ws}"
DISCOVERY_DIR="${DISCOVERY_DIR:-${CPPLIB_SRC_DIR}/discovery_protocol}"

# CMake build type (override if needed)
BUILD_TYPE="${BUILD_TYPE:-Release}"

# Number of parallel jobs
JOBS="${JOBS:-$(nproc)}"

echo "User: $(whoami)"
echo "Using CPPLIB_SRC_DIR=${CPPLIB_SRC_DIR}"
echo "Using ROS_WS=${ROS_WS}"
echo "Using DISCOVERY_DIR=${DISCOVERY_DIR}"

declare -a ERRORS

if [[ ! -d "${CPPLIB_SRC_DIR}" ]]; then
  echo "ERROR: CPPLIB_SRC_DIR '${CPPLIB_SRC_DIR}' does not exist." >&2
  ERRORS+=("missing_cpplib_src")
fi
if [[ ! -d "${DISCOVERY_DIR}" ]]; then
  echo "ERROR: discovery_protocol directory '${DISCOVERY_DIR}' not found." >&2
  ERRORS+=("missing_discovery_dir")
fi
if (( ${#ERRORS[@]} )); then
  exit 1
fi

echo "== Building discovery_protocol =="

cd "${DISCOVERY_DIR}" || exit 1

if [[ ! -d build ]]; then
  echo "Creating build directory..."
  mkdir build
fi

cd build || exit 1

# Configure (re-run only if cache missing or FORCE_RECONFIGURE set)
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

cd "${ROS_WS}" || exit 1

if [[ ! -d src ]]; then
  echo "ERROR: ROS workspace at '${ROS_WS}' missing 'src' directory." >&2
  exit 1
fi

# Clean optional (set CLEAN=1 to force)
if [[ "${CLEAN:-0}" == "1" ]]; then
  echo "Cleaning previous build/ install/ log/..."
  rm -rf build install log
fi

echo "Building modular_gateway_sender and offloading_latency_test_loopback..."
colcon build --packages-select modular_gateway_sender offloading_latency_test_loopback

# Safe sourcing helper to avoid 'set -u' unbound variable errors in generated scripts
safe_source() {
  local script="$1"
  if [[ -f "$script" ]]; then
    : "${COLCON_TRACE:=}"
    set +u
    . "$script"
    set -u
  else
    echo "WARNING: Tried to source missing script: $script" >&2
  fi
}

echo "Sourcing workspace (safe)..."
safe_source "${ROS_WS}/install/setup.bash"

echo "Launching VHC stack..."
exec ros2 launch offloading_latency_test_loopback vch_launch.py