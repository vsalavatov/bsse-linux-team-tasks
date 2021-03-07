cmake_minimum_required(VERSION 3.10)
project(elf_parser)

set(CMAKE_CXX_STANDARD 17)

add_executable(my_ldd main.cpp elf_parser.cpp elf_parser.h)

enable_testing()
add_subdirectory(tests)