cmake_minimum_required(VERSION 3.16)
project(ToupcamApp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Find required Qt5 components
find_package(Qt5 REQUIRED COMPONENTS Core Widgets OpenGL)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link directories
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/lib)

# Source files
set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/CameraController.cpp
    src/CameraWidget.cpp
    src/ControlPanel.cpp
    src/IconButton.cpp
    src/ZoomController.cpp
    src/AutoExposureOverlay.cpp
)

# Header files
set(HEADERS
    src/MainWindow.h
    src/CameraController.h
    src/CameraWidget.h
    src/ControlPanel.h
    src/IconButton.h
    src/ZoomController.h
    src/AutoExposureOverlay.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Widgets
    Qt5::OpenGL
    toupcam
    pthread
    GL
)

# Set runtime path for finding libtoupcam.so
set_target_properties(${PROJECT_NAME} PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Create output directories
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/photos)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/recordings)