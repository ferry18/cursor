# Toupcam ATR2600M Camera Application

A Qt-based camera control application for the Touptek ATR2600M monochrome camera with advanced features.

## Features

### Camera Control
- **Resolution**: Fixed at 3104x2084 pixels (2x2 hardware binning)
- **Frame Rate**: ~35-38 fps with live FPS display
- **Mode**: RAW8 grayscale mode with optimized performance

### Control Panel (Left Side - 171px)
1. **LCG/HCG Toggle**: Switch between Low and High Conversion Gain modes
2. **High Full Well Mode**: Enable/disable high full well capacity
3. **Heating Control**: 4 levels (Off, 2, 3, 4)
4. **Cooling Control**: Temperature presets (10°C, 0°C, -15°C, -30°C)
5. **Denoise Control**: (Placeholder for future implementation)
6. **Low Noise Mode**: (Placeholder for future implementation)
7. **Shutter Button**: Capture PNG photos
8. **Record Button**: Start/stop PNG sequence recording

### Camera View Features
- **Zoom Control**:
  - Scroll wheel: Zoom to center (100% - 800%)
  - Ctrl + Scroll: Zoom to cursor position
  - Automatic snap to 100% when zooming out
  
- **Pan Control**:
  - Middle mouse button drag: Pan when zoomed in
  - Scroll wheel (when zoomed): Move up/down

- **Auto-Exposure**:
  - Default: 70% x 70% center rectangle
  - Left click: Set 300x300px AE rectangle at click position
  - iPhone-style exposure indicator animation
  - Double-click: Reset to default AE rectangle

- **Minimap**: Shows current view area when zoomed (bottom-right)

### File Management
- **Photos**: Saved as PNG in `photos/` directory
- **Recordings**: PNG sequences in `recordings/rec_YYYYMMDD_HHMMSS/`

## Camera Settings (Applied on Startup)
- IWR (Integrate While Read) readout mode
- High priority USB data retrieval
- Auto exposure time damping: 0ms
- Tail light: Disabled
- TEC cooling: Enabled at 10°C default

## Building

### Requirements
- Qt5 (Core, Widgets, OpenGL)
- CMake 3.16+
- C++17 compiler
- libtoupcam.so (ARM64)

### Build Instructions
```bash
./build.sh
```

## Running
```bash
# Make sure libtoupcam.so is in the lib/ directory
./build/ToupcamApp
```

## Usage Tips
1. The camera automatically initializes with optimal settings
2. Use Ctrl+Scroll for precise zoom control
3. Click anywhere on the feed to adjust exposure for that area
4. Temperature is displayed in the status bar
5. All captures are saved as lossless PNG files