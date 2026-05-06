#!/bin/bash
set -euo pipefail

echo "=== Starting Bridge Auto-Build and Launch ==="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [[ "${BRIDGE_CLEAN_BUILD:-0}" == "1" ]]; then
  echo "Clean build requested (BRIDGE_CLEAN_BUILD=1). Removing existing build directory..."
  rm -rf build
fi

if [ ! -d build ]; then
  echo "Creating build directory..."
  mkdir build
fi
cd build

# Configure (re-run cmake if cache missing or CMakeLists.txt newer)
if [ ! -f CMakeCache.txt ] || [ ../CMakeLists.txt -nt CMakeCache.txt ]; then
  echo "Configuring bridge with CMake..."
  cmake ..
else
  echo "CMake already configured. Skipping (delete build/ or set BRIDGE_CLEAN_BUILD=1 to force)."
fi

echo "Building bridge (parallel=$(nproc))..."
make -j"$(nproc)"

if [ -f ./bin/bridge ]; then
  echo "Build successful! Launching bridge..."
  exec ./bin/bridge
else
  echo "ERROR: bridge executable not found at ./bin/bridge after build!" >&2
  ls -R . || true
  exit 1
fi
