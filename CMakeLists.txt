cmake_minimum_required(VERSION 2.8)
project(aio_test)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

add_definitions(-std=c++11)

add_executable(aio_test main.cpp)

