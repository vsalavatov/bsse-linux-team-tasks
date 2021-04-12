cmake_minimum_required(VERSION 3.10)
project(bablib)

set(FUSE3_DIR ${CMAKE_SOURCE_DIR}/cmake/)

find_package(FUSE3 REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bablib src/main.cpp src/bablib.cpp)
target_link_libraries(bablib PRIVATE FUSE3::FUSE3)
