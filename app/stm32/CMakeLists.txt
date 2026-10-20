cmake_minimum_required(VERSION 3.20)
project(u-connectClientApp-stm32 C ASM)

# Cross build of the command layer for STM32 boards. Configure with
#   -DCMAKE_TOOLCHAIN_FILE=<repo>/u-connectClient/cmake/arm-none-eabi-gcc.cmake -DSTM32_BOARD=nucleo_h753zi
# (u-connectClientAppSTM32.cmd does this).

set(UCX_MODULE "NORA-W36X" CACHE STRING "Generated API variant in u-connectClient/ucx_api/generated")
include(../../u-connectClient/u-connectClient.cmake)

if(NOT DEFINED STM32_BOARD)
    set(STM32_BOARD "nucleo_h753zi" CACHE STRING "STM32 board")
endif()

set(EXAMPLE_COMMON_SRC
    ${U_CONNECT_CLIENT_UCX_API_SRC}
    ${U_CONNECT_CLIENT_AT_API_SRC}
)

include(../../u-connectClient/cmake/stm32.cmake)

set(APP_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
set(UCXAPP_STM32_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/ucxapp_main_stm32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ucxapp_platform_stm32.c
    ${APP_SRC}/ucxapp_io.c
    ${APP_SRC}/ucxapp_events.c
    ${APP_SRC}/ucxapp_store.c
    ${APP_SRC}/ucx_obfuscate.c
    ${APP_SRC}/ucxapp_hash.c
    ${APP_SRC}/ucxapp_cmd.c
    ${APP_SRC}/ucxapp_urc.c
    ${APP_SRC}/ucxapp_cmds_device.c
    ${APP_SRC}/ucxapp_cmds_wifi.c
    ${APP_SRC}/ucxapp_cmds_sock.c
    ${APP_SRC}/ucxapp_cmds_net.c
    ${APP_SRC}/ucxapp_cmds_sys.c
    ${APP_SRC}/ucxapp_cmds_mqtt.c
    ${APP_SRC}/ucxapp_cmds_bt.c
    ${APP_SRC}/ucxapp_cmds_cert.c
    ${APP_SRC}/ucxapp_cmds_gatt.c
)

string(TOLOWER "${STM32_BOARD}" _board)
set(UCXAPP_STM32_NAME "ucxapp_${_board}")
add_stm32_example(NAME ${UCXAPP_STM32_NAME} SOURCES ${UCXAPP_STM32_SOURCES})

target_include_directories(${UCXAPP_STM32_NAME}.elf PRIVATE ${APP_SRC})
# The command layer needs more heap than the examples (32 KB default); the F439ZI has only 192 KB of RAM.
if(STM32_BOARD STREQUAL "nucleo_f439zi")
    set(UCXAPP_HEAP_SIZE 49152)
else()
    set(UCXAPP_HEAP_SIZE 163840)
endif()
target_compile_definitions(${UCXAPP_STM32_NAME}.elf PRIVATE configTOTAL_HEAP_SIZE=${UCXAPP_HEAP_SIZE})
