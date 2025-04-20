cmake_minimum_required(VERSION 3.20.0)

set(BOARD esp32s3_devkitm)
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/esp32s3_node.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(tpms_node)

target_sources(app PRIVATE src/main.c)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common/can_protocol)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common/sensor_utils)
