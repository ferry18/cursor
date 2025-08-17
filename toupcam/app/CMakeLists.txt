cmake_minimum_required(VERSION 3.18)

project(ToupcamApp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Qt 6
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets Svg)

# Sources
set(APP_SOURCES
	src/main.cpp
	src/MainWindow.cpp
	src/CameraController.cpp
	src/GLCameraView.cpp
	src/FrameWriter.cpp
	src/ImageNumbering.cpp
)

set(APP_HEADERS
	include/MainWindow.h
	include/CameraController.h
	include/GLCameraView.h
	include/FrameWriter.h
	include/ImageNumbering.h
)

set(APP_RESOURCES
	resources/icons.qrc
)

qt_add_executable(${PROJECT_NAME}
	${APP_SOURCES}
	${APP_HEADERS}
	${APP_RESOURCES}
)

# Toupcam SDK include and library directories
set(TOUPCAM_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/../toupcamsdk/inc")
# Prefer absolute path per workspace layout
set(TOUPCAM_LIB_DIR "/workspace/toupcam/toupcamsdk/linux/arm64/")

include_directories(${TOUPCAM_INCLUDE_DIR})
link_directories(${TOUPCAM_LIB_DIR})

# Link Qt6 and toupcam shared lib

target_link_libraries(${PROJECT_NAME}
	PRIVATE
	Qt6::Widgets
	Qt6::OpenGLWidgets
	Qt6::Svg
	pthread
	# toupcam shared library (-ltoupcam)
	toupcam
)

# RPATH so the runtime finds libtoupcam.so
set_target_properties(${PROJECT_NAME} PROPERTIES
	BUILD_WITH_INSTALL_RPATH TRUE
	INSTALL_RPATH "${TOUPCAM_LIB_DIR}"
	BUILD_RPATH "${TOUPCAM_LIB_DIR}"
)

# Install (optional)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)