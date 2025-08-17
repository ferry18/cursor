#!/bin/bash

# Build script with Qt5 fallback for Toupcam application

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${GREEN}Building Toupcam Application...${NC}"

# Clean previous build
if [ -d "build" ]; then
    echo -e "${YELLOW}Removing previous build directory...${NC}"
    rm -rf build
fi

# Create library directory
mkdir -p ../toupcamsdk/linux/arm64

# Check for Qt6
if pkg-config --exists Qt6Core 2>/dev/null; then
    echo -e "${GREEN}Qt6 found, using Qt6${NC}"
    USE_QT5=OFF
else
    echo -e "${YELLOW}Qt6 not found, checking for Qt5...${NC}"
    if pkg-config --exists Qt5Core 2>/dev/null; then
        echo -e "${GREEN}Qt5 found, using Qt5${NC}"
        USE_QT5=ON
    else
        echo -e "${RED}Neither Qt6 nor Qt5 found!${NC}"
        echo "Please install Qt development packages:"
        echo "  For Qt6: sudo apt install qt6-base-dev libqt6opengl6-dev"
        echo "  For Qt5: sudo apt install qtbase5-dev qt5-default"
        exit 1
    fi
fi

# Create build directory
mkdir build
cd build

# Configure with CMake
echo -e "${YELLOW}Configuring with CMake...${NC}"
if [ "$USE_QT5" = "ON" ]; then
    cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_QT5=ON
else
    cmake .. -DCMAKE_BUILD_TYPE=Release
fi

# Build
echo -e "${YELLOW}Building...${NC}"
make -j$(nproc)

echo -e "${GREEN}Build complete!${NC}"
echo ""
echo -e "${YELLOW}Don't forget to:${NC}"
echo "1. Place libtoupcam.so in: ../toupcamsdk/linux/arm64/"
echo "2. Install udev rules (if not done already)"
echo ""
echo -e "${GREEN}Run with: ./toupcam_app${NC}"