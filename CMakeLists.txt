cmake_minimum_required(VERSION 3.10)
project(bablib)

set(FUSE3_DIR ${CMAKE_SOURCE_DIR}/cmake/)

find_package(FUSE3 REQUIRED)

set(CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++20")

add_executable(bablib src/main.cpp src/bablib.cpp)
target_link_libraries(bablib PRIVATE FUSE3::FUSE3)
