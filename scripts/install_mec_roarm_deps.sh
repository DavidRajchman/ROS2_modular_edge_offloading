#!/bin/bash
# Install dependencies for RoArm-M1 GUI and simulation on MEC (Laptop)

set -e

echo "Updating package lists..."
sudo apt-get update

echo "Installing ROS 2 Jazzy dependencies for robotic arm GUI..."
sudo apt-get install -y \
    ros-jazzy-joint-state-publisher-gui \
    ros-jazzy-joint-state-publisher \
    ros-jazzy-robot-state-publisher \
    ros-jazzy-xacro \
    ros-jazzy-rviz2 \
    python3-serial

echo "Dependencies installed successfully."
