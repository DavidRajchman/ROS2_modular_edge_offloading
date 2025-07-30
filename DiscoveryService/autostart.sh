#!/bin/bash
set -e  # Exit on any error

echo "=== Starting Discovery Service Auto-Build and Launch ==="

# Navigate to build directory
cd /home/ubuntu/DiscoveryService/build

# Configure with cmake
echo "Configuring with cmake..."
cmake ..

# Build the project
echo "Building discovery service..."
make -j$(nproc)

# Check if executable exists
if [ -f "./DiscoveryService" ]; then
    echo "Build successful! Starting discovery service..."
    ./DiscoveryService
else
    echo "ERROR: DiscoveryService executable not found after build!"
    exit 1
fi
