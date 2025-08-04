#!/bin/bash
set -e  # Exit on any error

echo "=== Starting Offloading Manager Auto-Launch ==="

# Navigate to OffloadingManager directory
cd /home/ubuntu/OffloadingManager

# Check if main.py exists
if [ ! -f "main.py" ]; then
    echo "ERROR: main.py not found in /home/ubuntu/OffloadingManager/"
    exit 1
fi

# Install Python dependencies if requirements.txt exists
if [ -f "requirements.txt" ]; then
    echo "Installing Python dependencies..."
    pip3 install -r requirements.txt
fi

# Start the Offloading Manager
echo "Starting Offloading Manager..."
python3 main.py --config /home/ubuntu/OffloadingManager/config.json --log-level INFO
