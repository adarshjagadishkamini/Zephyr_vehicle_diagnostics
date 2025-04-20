cmake_minimum_required(VERSION 3.20.0)

set(BOARD native_posix)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vehicle_tracking_tests)

target_sources(app PRIVATE
    sensor_validation_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/sensor_utils/sensor_validation.c
)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/sensor_utils
)
