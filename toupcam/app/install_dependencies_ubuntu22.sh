#!/bin/bash

echo "Installing Qt5 and other dependencies for Toupcam application (Ubuntu 22.04+)..."

# Update package list
sudo apt update

# Install Qt5 development packages (qt5-default is deprecated in Ubuntu 22.04+)
sudo apt install -y \
    qtbase5-dev \
    qtbase5-dev-tools \
    qt5-qmake \
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

# Set Qt5 as default if multiple Qt versions are installed
if command -v qtchooser >/dev/null 2>&1; then
    export QT_SELECT=qt5
    echo "export QT_SELECT=qt5" >> ~/.bashrc
fi

echo "Dependencies installed successfully!"
echo ""
echo "You can now run ./build.sh to compile the application."