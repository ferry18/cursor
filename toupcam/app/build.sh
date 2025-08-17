#!/bin/bash

# Build script for Toupcam Qt6 application

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}Building Toupcam Application...${NC}"

# Check if build directory exists
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

echo -e "${GREEN}Build complete!${NC}"
echo ""
echo -e "${YELLOW}Important: Make sure to install udev rules before running:${NC}"
echo "sudo cp ../toupcamsdk/linux/udev/99-toupcam.rules /etc/udev/rules.d/"
echo "sudo udevadm control --reload && sudo udevadm trigger"
echo ""
echo -e "${YELLOW}Also ensure Qt6 is installed:${NC}"
echo "sudo apt install qt6-base-dev libqt6opengl6-dev"
echo ""
echo -e "${GREEN}To run the application:${NC}"
echo "./toupcam_app"