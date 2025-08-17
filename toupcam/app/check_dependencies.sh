#!/bin/bash

# Check dependencies for Toupcam Qt6 application

echo "Checking build dependencies..."

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo "❌ CMake not found. Install with: sudo apt install cmake"
else
    echo "✅ CMake found: $(cmake --version | head -n1)"
fi

# Check for Qt6
if pkg-config --exists Qt6Core 2>/dev/null; then
    echo "✅ Qt6 Core found"
else
    echo "❌ Qt6 Core not found. Install with: sudo apt install qt6-base-dev"
fi

if pkg-config --exists Qt6Widgets 2>/dev/null; then
    echo "✅ Qt6 Widgets found"
else
    echo "❌ Qt6 Widgets not found. Install with: sudo apt install qt6-base-dev"
fi

if pkg-config --exists Qt6OpenGL 2>/dev/null; then
    echo "✅ Qt6 OpenGL found"
else
    echo "❌ Qt6 OpenGL not found. Install with: sudo apt install libqt6opengl6-dev"
fi

if pkg-config --exists Qt6OpenGLWidgets 2>/dev/null; then
    echo "✅ Qt6 OpenGLWidgets found"
else
    echo "❌ Qt6 OpenGLWidgets not found. Install with: sudo apt install libqt6opengl6-dev"
fi

# Check for compiler
if ! command -v g++ &> /dev/null; then
    echo "❌ g++ not found. Install with: sudo apt install build-essential"
else
    echo "✅ g++ found: $(g++ --version | head -n1)"
fi

# Check for libtoupcam.so
if [ -f "../toupcamsdk/linux/arm64/libtoupcam.so" ]; then
    echo "✅ libtoupcam.so found"
else
    echo "❌ libtoupcam.so not found at ../toupcamsdk/linux/arm64/"
fi

# Check for udev rules
if [ -f "/etc/udev/rules.d/99-toupcam.rules" ]; then
    echo "✅ udev rules installed"
else
    echo "⚠️  udev rules not installed. Install with:"
    echo "   sudo cp ../toupcamsdk/linux/udev/99-toupcam.rules /etc/udev/rules.d/"
    echo "   sudo udevadm control --reload && sudo udevadm trigger"
fi

echo ""
echo "Dependency check complete!"