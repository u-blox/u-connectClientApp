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

# Non-Windows hosts (Linux): command-line app built from the portable command layer only

if(NOT WIN32)

  find_package(Threads REQUIRED)



  add_executable(ucxapp

    src/ucxapp_main.c

    src/ucxapp_events.c
    src/ucxapp_thread.c
    src/ucxapp_cmds_web.c
    src/u_webserver.c

    src/ucxapp_io.c

    src/ucxapp_io_stdio.c
    src/ucxapp_store.c
    src/ucx_obfuscate.c
    src/ucxapp_hash.c

    src/ucxapp_cmd.c

    src/ucxapp_urc.c

    src/ucxapp_cmds_device.c

    src/ucxapp_cmds_wifi.c

    src/ucxapp_cmds_sock.c

    src/ucxapp_cmds_net.c

    src/ucxapp_cmds_sys.c

    src/ucxapp_cmds_mqtt.c

    src/ucxapp_cmds_bt.c
    src/ucxapp_cmds_cert.c
    src/ucxapp_cmds_fw.c
    src/ucxapp_cmds_gatt.c
    src/ucxapp_cmds_qr.c
    src/ucxapp_cmds_hid.c
    third-party/qrcodegen/qrcodegen.c

    src/ucxapp_platform_posix.c

    ../u-connectClient/ports/os/u_port_posix.c

    ../u-connectClient/ports/uart/u_port_uart_linux.c

    ${U_CONNECT_CLIENT_UCX_API_SRC}

    ${U_CONNECT_CLIENT_AT_API_SRC}

  )

  target_compile_options(ucxapp PRIVATE -Wall -Wextra)

  option(UCXAPP_COVERAGE "Instrument ucxapp with gcov" OFF)
  if(UCXAPP_COVERAGE)
    target_compile_options(ucxapp PRIVATE --coverage -O0 -g)
    target_link_options(ucxapp PRIVATE --coverage)
  endif()

  target_compile_definitions(ucxapp PRIVATE U_PORT_POSIX
    APP_VERSION_MAJOR=${APP_VERSION_MAJOR}
    APP_VERSION_MINOR=${APP_VERSION_MINOR}
    APP_VERSION_PATCH=${APP_VERSION_PATCH}
    APP_VERSION_BUILD=${APP_VERSION_BUILD})

  target_include_directories(ucxapp PRIVATE

    ${U_CONNECT_CLIENT_INC}

    ${U_CONNECT_CLIENT_PORT_DIR}

    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third-party

  )

  target_link_libraries(ucxapp PRIVATE Threads::Threads)

  return()

endif()

set(U_EXAMPLE_UART "COM3" CACHE STRING "The UART device to use")

# Port layer files for Windows (from u-connectClient submodule)
set(PORT_OS_SRC ../u-connectClient/ports/os/u_port_windows.c)
set(PORT_UART_SRC ../u-connectClient/ports/uart/u_port_uart_windows.c)

# FTDI D2XX is loaded at run time from the DLL embedded by the .rc file

# u-connectClientApp Application
add_executable(u-connectClientApp
  src/ucxapp_main.c
  src/ucxapp_urc.c
  src/ucx_obfuscate.c
  src/ucxapp_events.c
  src/ucxapp_thread.c
  src/ucxapp_cmds_web.c
  src/u_webserver.c
  src/ucxapp_io.c
  src/ucxapp_io_stdio.c
  src/ucxapp_store.c
  src/ucxapp_hash.c
  src/ucxapp_platform_win.c
  src/ucxapp_cmd.c
  src/ucxapp_cmds_device.c
  src/ucxapp_cmds_wifi.c
  src/ucxapp_cmds_sock.c
  src/ucxapp_cmds_net.c
  src/ucxapp_cmds_sys.c
  src/ucxapp_cmds_mqtt.c
  src/ucxapp_cmds_bt.c
  src/ucxapp_cmds_cert.c
  src/ucxapp_cmds_fw.c
  src/ucxapp_cmds_gatt.c
  src/ucxapp_cmds_qr.c
  src/ucxapp_cmds_hid.c
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
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third-party
  ${CMAKE_CURRENT_SOURCE_DIR} 
  ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(u-connectClientApp PRIVATE setupapi)

# Set different output names for Debug and Release builds
set_target_properties(u-connectClientApp PROPERTIES
  OUTPUT_NAME_DEBUG "u-connectClientApp-debug"
  OUTPUT_NAME_RELEASE "u-connectClientApp"
)
