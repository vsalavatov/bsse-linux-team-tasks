cmake_minimum_required(VERSION 3.17)
project(elf_parser)

set(CMAKE_CXX_STANDARD 17)

add_executable(elf_parser main.cpp elf_parser.cpp elf_parser.h)