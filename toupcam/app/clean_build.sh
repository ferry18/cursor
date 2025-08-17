#!/bin/bash

# Clean build script for Toupcam Qt6 application

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Clean Build for Toupcam Application...${NC}"

# Clean previous build
if [ -d "build" ]; then
    echo -e "${YELLOW}Removing previous build directory...${NC}"
    rm -rf build
fi

# Create library directory
mkdir -p ../toupcamsdk/linux/arm64

# Create build directory
echo "Creating build directory..."
mkdir build
cd build

# Configure with CMake, disabling the problematic autogen parallelization
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_AUTOGEN_PARALLEL=1

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

echo -e "${GREEN}Build complete!${NC}"
echo ""
echo -e "${YELLOW}Important steps:${NC}"
echo ""
echo "1. Place your libtoupcam.so file:"
echo "   cp /path/to/your/libtoupcam.so ../toupcamsdk/linux/arm64/"
echo ""
echo "2. Install udev rules:"
echo "   sudo cp ../toupcamsdk/linux/udev/99-toupcam.rules /etc/udev/rules.d/"
echo "   sudo udevadm control --reload && sudo udevadm trigger"
echo ""
echo -e "${GREEN}To run the application:${NC}"
echo "   ./toupcam_app"