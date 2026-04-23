cmake_minimum_required(VERSION 3.10)
project(WallE_Control)

set(CMAKE_CXX_STANDARD 17)

# Dependencies
# Install on Jetson: sudo apt-get install libi2c-dev libevdev-dev
find_package(Parameters REQUIRED) # Placeholder if needed, but standard approach:

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(/usr/include/libevdev-1.0) # Standard path on Jetson/Ubuntu

# Source files
file(GLOB SOURCES "src/*.cpp")

# Executable
add_executable(walle_control ${SOURCES})

# Libraries
# Link against i2c (standard linux i2c-dev) and evdev (libevdev)
target_link_libraries(walle_control i2c evdev pthread)

# Compiler flags for Jetson (optimizations)
if(UNIX)
    target_compile_options(walle_control PRIVATE -Wall -O2)
endif()
