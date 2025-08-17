cmake_minimum_required(VERSION 3.16)
project(ToupcamApp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Set AUTOGEN build directory to avoid path issues
set(CMAKE_AUTOGEN_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/autogen)

# Find Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets OpenGL OpenGLWidgets)

# Source files
set(SOURCES
    src/main.cpp
    src/app_main_window.cpp
    src/camera_controller.cpp
    src/image_view.cpp
    src/frame_buffer.cpp
    src/recording_manager.cpp
    src/ui_panel.cpp
)

set(HEADERS
    src/app_main_window.h
    src/camera_controller.h
    src/image_view.h
    src/frame_buffer.h
    src/recording_manager.h
    src/ui_panel.h
    src/utils.h
)

# Resources
set(RESOURCES
    resources/resources.qrc
)

# Create executable
add_executable(toupcam_app ${SOURCES} ${HEADERS} ${RESOURCES})

# Set resource file property to handle special characters
set_source_files_properties(${RESOURCES} PROPERTIES 
    AUTORCC_OPTIONS "--no-compress"
)

# Include directories
target_include_directories(toupcam_app PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../toupcamsdk/inc
)

# Link libraries
target_link_libraries(toupcam_app
    Qt6::Core
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    ${CMAKE_CURRENT_SOURCE_DIR}/../toupcamsdk/linux/arm64/libtoupcam.so
)

# Set RPATH for runtime library loading
set_target_properties(toupcam_app PROPERTIES
    INSTALL_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/../toupcamsdk/linux/arm64"
    BUILD_WITH_INSTALL_RPATH TRUE
)

# Copy udev rules reminder
add_custom_command(TARGET toupcam_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Remember to install udev rules:"
    COMMAND ${CMAKE_COMMAND} -E echo "sudo cp ${CMAKE_CURRENT_SOURCE_DIR}/../toupcamsdk/linux/udev/99-toupcam.rules /etc/udev/rules.d/"
    COMMAND ${CMAKE_COMMAND} -E echo "sudo udevadm control --reload && sudo udevadm trigger"
)