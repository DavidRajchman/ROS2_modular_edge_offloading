#!/bin/bash
set -e  # Exit on any error

echo "=== Starting Bridge Auto-Build and Launch ==="

# Navigate to bridge source directory (this script is located in it)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Create / enter build directory
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi
cd build

# Configure
echo "Configuring bridge with CMake..."
cmake ..

# Build
echo "Building bridge..."
make -j$(nproc)

# Run
if [ -f "./bin/bridge" ]; then
    echo "Build successful! Launching bridge..."
    exec ./bin/bridge
else
    echo "ERROR: bridge executable not found at ./bin/bridge after build!" >&2
    exit 1
fi
