# ##############################################################################################
# # CMakeLists.txt                                                                             #
# # Copyright                acontis technologies GmbH, Weingarten, Germany                    #
# # Response                 Paul Bussmann                                                     #
# # Description              EC-Master / EC-Monitor / EC-Simulator projects                    #
# ##############################################################################################
# Example usage to build Linux x64 Debug:
# $ cmake -DEC_OS=Linux -DEC_ARCH=x64 -DCMAKE_BUILD_TYPE=Debug .
# $ cmake --build .
# ##############################################################################################

cmake_minimum_required (VERSION 3.5)
project(EcEmbedded)

# ##############################################################################################
# check parameters
if (NOT DEFINED EC_OS)
  message(FATAL_ERROR "EC_OS not set!")
endif()
if (NOT DEFINED EC_ARCH)
  message(FATAL_ERROR "EC_ARCH not set!")
endif()

# ##############################################################################################
set(EC_TOP_LEVEL_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE STRING "")
set(BUILDOUTPUT ${CMAKE_CURRENT_LIST_DIR} CACHE STRING "")
MESSAGE(STATUS "Building ${EC_OS}/${EC_ARCH}/${CMAKE_BUILD_TYPE} in ${BUILDOUTPUT}")
if (DEFINED CMAKE_TOOLCHAIN_FILE)
MESSAGE(STATUS "CMAKE_TOOLCHAIN_FILE: ${CMAKE_TOOLCHAIN_FILE}")
endif()

if (CMAKE_BUILD_TYPE STREQUAL Protected)
    set(ADDITIONAL_COMPILE_DEFINITIONS_BUILD_CONFIGURATION PROTECTED_VERSION)
endif()

# ##############################################################################################
# OS settings
if (EC_OS STREQUAL LxWin)
add_subdirectory("${EC_TOP_LEVEL_DIR}/Workspace/Linux")
else()
add_subdirectory("${EC_TOP_LEVEL_DIR}/Workspace/${EC_OS}")
endif()

if (NOT DEFINED EC_SDK_LIB_DIR)
set (EC_SDK_LIB_DIR ${EC_TOP_LEVEL_DIR}/SDK/LIB/${EC_OS_LAYER_INC_NAME} CACHE STRING "")
endif()
set(EC_LINK_DIRECTORIES ${EC_SDK_LIB_DIR}/${EC_ARCH}/${CMAKE_BUILD_TYPE} ${EC_SDK_LIB_DIR}/${EC_ARCH} CACHE STRING "" FORCE)

link_directories(${EC_LINK_DIRECTORIES} ${CMAKE_LINK_DIRECTORIES_ARCH})

# ##############################################################################################
# OS-independent settings
set(EC_OS_LAYER_INC SDK/INC/${EC_OS_LAYER_INC_NAME})

if ((EC_OS STREQUAL LxWin) AND (EXISTS ${EC_TOP_LEVEL_DIR}/SDK/Examples))
    # Debug output directories for .exe, .so, .a
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BUILDOUTPUT}/Bin/Linux/${EC_ARCH}/Debug)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BUILDOUTPUT}/Bin/Linux/${EC_ARCH}/Debug)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${BUILDOUTPUT}/SDK/LIB/Linux/${EC_ARCH}/Debug)

    # output directories for .exe, .so, .a
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILDOUTPUT}/Bin/Linux/${EC_ARCH}/${CMAKE_BUILD_TYPE})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BUILDOUTPUT}/Bin/Linux/${EC_ARCH}/${CMAKE_BUILD_TYPE})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BUILDOUTPUT}/SDK/LIB/Linux/${EC_ARCH}/${CMAKE_BUILD_TYPE})
else()
    # Debug output directories for .exe, .so, .a
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BUILDOUTPUT}/Bin/${EC_OS}/${EC_ARCH}/Debug)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BUILDOUTPUT}/Bin/${EC_OS}/${EC_ARCH}/Debug)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${BUILDOUTPUT}/SDK/LIB/${EC_OS}/${EC_ARCH}/Debug)

    # output directories for .exe, .so, .a
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILDOUTPUT}/Bin/${EC_OS}/${EC_ARCH}/${CMAKE_BUILD_TYPE})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BUILDOUTPUT}/Bin/${EC_OS}/${EC_ARCH}/${CMAKE_BUILD_TYPE})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BUILDOUTPUT}/SDK/LIB/${EC_OS}/${EC_ARCH}/${CMAKE_BUILD_TYPE})
endif()

include_directories(${EC_OS_LAYER_INC} SDK/INC Sources/Common ${CMAKE_INCLUDE_DIRECTORIES_ARCH})

# ##############################################################################################
# Third Party Libraries

# mdf4lib
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/mdf4lib/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/mdf4lib)
endif()

# C:/eclipse-paho-mqtt vs. /opt/eclipse-paho-mqtt
if ("${ECLIPSE_PAHO_DIR}" STREQUAL "")
    if (EXISTS "C:/eclipse-paho-mqtt")
        set(ECLIPSE_PAHO_DIR C:/eclipse-paho-mqtt)
    endif()
endif()

# ##############################################################################################
# Core Libraries

# EC-Master Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMaster/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMaster)
endif()

# EC-Monitor Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMonitor/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMonitor)
endif()

# EC-Simulator Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcSimulator/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcSimulator)
endif()

# #################### Feature packs ###########################################################
# EC-DAQ Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcDaq/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcDaq)
endif()

# EC-EAP Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcEap/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcEap)
endif()

# EC-Motion Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMotionControl/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMotionControl)
endif()

# EC-Motion-Advanced Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMotionAdvanced/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMotionAdvanced)
endif()

# ##############################################################################################
# RAS Libraries

# EC-Master RAS Client Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterRasClient/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterRasClient)
endif()

# EC-Master RAS Server Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterRasServer/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterRasServer)
endif()

# EC-Monitor RAS Server Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMonitorRasServer/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMonitorRasServer)
endif()

# EC-Simulator RAS Client Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcSimulatorRasClient/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcSimulatorRasClient)
endif()

# EC-Simulator RAS Server Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcSimulatorRasServer/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcSimulatorRasServer)
endif()

# EC-Master MQTT Server Core Library
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterMqttServer/CMakeLists.txt") 
    add_subdirectory(Workspace/cmake/EcMasterMqttServer)
endif()

# ##############################################################################################
# Link Layer

# emllAlteraTSE
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllAlteraTSE/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllAlteraTSE)
endif()

# emllBcmGenet
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllBcmGenet/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllBcmGenet)
endif()

# emllBcmNetXtreme
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllBcmNetXtreme/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllBcmNetXtreme)
endif()

# emllCCAT
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllCCAT/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllCCAT)
endif()

# emllCPSW
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllCPSW/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllCPSW)
endif()

# emllCPSWG
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllCPSWG/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllCPSWG)
endif()

# emllDpdk
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllDpdk/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllDpdk)
endif()

# emllDW3504
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllDW3504/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllDW3504)
endif()

# emllEG20T
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllEG20T/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllEG20T)
endif()

# emllEom
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllEom/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllEom)
endif()

# emllETSEC
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllETSEC/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllETSEC)
endif()

# emllFslFec
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllFslFec/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllFslFec)
endif()

# emllGEM
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllGEM/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllGEM)
endif()

# emllI8254x
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllI8254x/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllI8254x)
endif()

# emllI8255x
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllI8255x/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllI8255x)
endif()

# emllICSS
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllICSS/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllICSS)
endif()

# emllIntelGbe
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllIntelGbe/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllIntelGbe)
endif()

# emllLAN743x
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllLAN743x/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllLAN743x)
endif()

# emllMultiplier
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllMultiplier/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllMultiplier)
endif()

# emllR6040
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllR6040/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllR6040)
endif()

# emllRemote
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllRemote/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllRemote)
endif()

# emllRTL8139
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllRTL8139/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllRTL8139)
endif()

# emllRTL8169
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllRTL8169/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllRTL8169)
endif()

  # emllATL2 (Marvell/Aquantia AQC113)
  if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllATL2/CMakeLists.txt")
      add_subdirectory(Workspace/cmake/emllATL2)
  endif()

# # emllSHEth
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllSHEth/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllSHEth)
endif()

# emllSimulator
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllSimulator/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllSimulator)
endif()

# emllSockRaw
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllSockRaw/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllSockRaw)
endif()

# emllSockXdp
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllSockXdp/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllSockXdp)
endif()

# emllVlan
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/emllVlan/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/emllVlan)
endif()

# ##############################################################################################
# Wrappers

# EcWrapper
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcWrapper/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcWrapper)
endif()

# ##############################################################################################
# Applications

# EcEapDemo
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcEapDemo/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcEapDemo)
endif()

# EcEoeGateway
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcEoeGateway/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcEoeGateway)
endif()

# EcMasterDemo
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemo/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemo)
endif()

# EcMasterDemoC
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoC/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoC)
endif()

# EcMasterDemoDaq
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoDaq/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoDaq)
endif()

# EcMasterDemoDc
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoDc/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoDc)
endif()

# EcMasterDemoEom
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoEom/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoEom)
endif()

# EcMasterDemoMasterRed
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoMasterRed/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoMasterRed)
endif()

# EcMasterDemoMinimal
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoMinimal/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoMinimal)
endif()

# EcMasterDemoMotion
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoMotion/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoMotion)
endif()

# EcMasterDemoMotionAdvanced
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoMotionAdvanced/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoMotionAdvanced)
endif()

# EcMasterDemoMqttServer
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoMqttServer/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoMqttServer)
endif()

# EcMasterDemoMulti
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoMulti/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoMulti)
endif()

# EcMasterDemoRasClient
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoRasClient/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoRasClient)
endif()

# EcMasterDemoRasServer
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoRasServer/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoRasServer)
endif()

# EcMasterDemoSyncSm
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMasterDemoSyncSm/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMasterDemoSyncSm)
endif()

# EcMonitorDemo
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMonitorDemo/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMonitorDemo)
endif()

# EcMonitorDemoMqttServer
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcMonitorDemoMqttServer/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcMonitorDemoMqttServer)
endif()

# EcSimulatorHilDemo
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcSimulatorHilDemo/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcSimulatorHilDemo)
endif()

# EcSimulatorHilDemoMotion
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcSimulatorHilDemoMotion/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcSimulatorHilDemoMotion)
endif()

# EcSimulatorSilDemo
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcSimulatorSilDemo/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/EcSimulatorSilDemo)
endif()

# EcSimulatorSilDemoMotion
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcSimulatorSilDemoMotion/CMakeLists.txt" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Examples/EcMasterDemoMotion")
    add_subdirectory(Workspace/cmake/EcSimulatorSilDemoMotion)
endif()

# EcWinDemo
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/EcWinDemo/CMakeLists.txt" AND EC_OS STREQUAL LxWin)
    add_subdirectory(Workspace/cmake/EcWinDemo)
endif()

# EC-Master / EC-Monitor / EC-Simulator Tests
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Workspace/cmake/Tests/CMakeLists.txt")
    add_subdirectory(Workspace/cmake/Tests)
endif()



# ###END OF FILE################################################################################
