cmake_minimum_required(VERSION 3.10)
project(WallECppController)

set(CMAKE_CXX_STANDARD 17)

# Najdi knihovnu hidapi pro komunikaci s myší
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIDAPI REQUIRED hidapi-hidraw)

include_directories(include ${HIDAPI_INCLUDE_DIRS})

add_executable(walle_controller
    src/main.cpp
    src/motor.cpp
    src/encoder.cpp
)

target_link_libraries(walle_controller ${HIDAPI_LIBRARIES} pthread)
