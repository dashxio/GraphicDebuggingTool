cmake_minimum_required(VERSION 3.12)

project(TestCpp LANGUAGES CXX)

include(compile_cxx17.cmake)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

#find_package(fmt REQUIRED)
#find_package(Boost REQUIRED COMPONENTS locale)

add_executable(test_occt main.cpp src/func_map.cpp)
