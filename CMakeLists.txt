cmake_minimum_required(VERSION 2.8)
project(hello_stap)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_definitions(-std=c++11)

add_executable(hello_stap main.cpp)

