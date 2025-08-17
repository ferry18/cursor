#!/bin/bash

echo "Installing Qt5 and other dependencies for Toupcam application..."

# Update package list
sudo apt update

# Install Qt5 development packages
sudo apt install -y \
    qt5-default \
    qtbase5-dev \
    qtbase5-dev-tools \
    libqt5opengl5-dev \
    libqt5widgets5 \
    libqt5gui5 \
    libqt5core5a \
    libgl1-mesa-dev \
    build-essential \
    cmake \
    pkg-config

# For ARM64 systems, you might also need
sudo apt install -y \
    libgles2-mesa-dev \
    libgbm-dev

echo "Dependencies installed successfully!"
echo ""
echo "You can now run ./build.sh to compile the application."