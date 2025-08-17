#!/bin/bash

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)

echo "Build complete. Executable: build/ToupcamApp"
echo "Note: Make sure libtoupcam.so is in the lib/ directory before running."