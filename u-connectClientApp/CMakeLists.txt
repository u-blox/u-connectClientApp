cmake_minimum_required(VERSION 3.10)
project(u-connectClientApp)

# Include u-connectClient library definitions from submodule
set(UCX_MODULE "NORA-W36X" CACHE STRING "Generated API variant in u-connectClient/ucx_api/generated")
include(../u-connectClient/u-connectClient.cmake)

# ============================================================================
# Auto-versioning: Use timestamp-based build number
# ============================================================================
set(APP_VERSION_MAJOR 3)
set(APP_VERSION_MINOR 4)
set(APP_VERSION_PATCH 0)
set(APP_VERSION_BUILD 0)

# Generate build number from current date: YYMMDD (e.g., 251204 = Dec 4, 2025)
# This is the most common CalVer format on GitHub
string(TIMESTAMP APP_VERSION_BUILD "%y%m%d")

message(STATUS "Build version: ${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}.${APP_VERSION_BUILD} (timestamp-based)")

# Generate version header
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/version.h
  @ONLY
)

# Set binary output directory to bin/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Windows-specific settings
if(NOT WIN32)
  message(FATAL_ERROR "u-connectClientApp requires Windows")
endif()

set(U_EXAMPLE_UART "COM3" CACHE STRING "The UART device to use")

# Port layer files for Windows (from u-connectClient submodule)
set(PORT_OS_SRC ../u-connectClient/ports/os/u_port_windows.c)
set(PORT_UART_SRC ../u-connectClient/ports/uart/u_port_uart_windows.c)

# FTDI D2XX library support
set(FTDI_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ftdi")
set(FTDI_LIBRARY "${CMAKE_CURRENT_SOURCE_DIR}/third-party/ftdi/ftd2xx.lib")

# u-connectClientApp Application (Monolithic)
add_executable(u-connectClientApp
  u-connectClientApp.c
  src/ucx_obfuscate.c
  src/ucxapp_events.c
  src/ucxapp_io.c
  src/ucxapp_io_stdio.c
  src/ucxapp_platform_win.c
  src/ucxapp_cmd.c
  src/ucxapp_cmds_device.c
  src/ucxapp_cmds_wifi.c
  src/ucxapp_cmds_sock.c
  src/ucxapp_cmds_net.c
  src/ucxapp_cmds_sys.c
  src/ucxapp_cmds_mqtt.c
  src/ucxapp_cmds_bt.c
  ${PORT_OS_SRC}
  ${PORT_UART_SRC}
  u-connectClientApp.rc
  third-party/qrcodegen/qrcodegen.c
  ${U_CONNECT_CLIENT_UCX_API_SRC}
  ${U_CONNECT_CLIENT_AT_API_SRC}
)

target_compile_options(u-connectClientApp PRIVATE /W4 /FS /wd4200)

target_compile_definitions(u-connectClientApp PRIVATE
  U_PORT_WINDOWS
  U_CX_PORT_HEADER_FILE="os/u_port_windows.h"
  U_CX_XMODEM_FILE_SUPPORT=1
  _CRT_SECURE_NO_WARNINGS
  U_EXAMPLE_UART="${U_EXAMPLE_UART}"
  APP_VERSION_MAJOR=${APP_VERSION_MAJOR}
  APP_VERSION_MINOR=${APP_VERSION_MINOR}
  APP_VERSION_PATCH=${APP_VERSION_PATCH}
  APP_VERSION_BUILD=${APP_VERSION_BUILD}
)

target_include_directories(u-connectClientApp PUBLIC 
  ${U_CONNECT_CLIENT_INC} 
  ${U_CONNECT_CLIENT_PORT_DIR} 
  ${FTDI_INCLUDE_DIR} 
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(u-connectClientApp PRIVATE setupapi ${FTDI_LIBRARY})

# Set different output names for Debug and Release builds
set_target_properties(u-connectClientApp PROPERTIES
  OUTPUT_NAME_DEBUG "u-connectClientApp-debug"
  OUTPUT_NAME_RELEASE "u-connectClientApp"
)
