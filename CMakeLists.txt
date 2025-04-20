cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(vehicle_tracking)

add_subdirectory(common)
add_subdirectory(vcu)
add_subdirectory(temp_node)
add_subdirectory(gps_node)
add_subdirectory(collision_node)
add_subdirectory(battery_node)
add_subdirectory(brake_node)
add_subdirectory(tpms_node)
