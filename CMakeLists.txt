cmake_minimum_required(VERSION 3.12)

project(TestOcct LANGUAGES CXX)

include(compile_cxx17.cmake)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

find_package(Boost REQUIRED COMPONENTS locale)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets OpenGL Concurrent)
find_package(OpenCASCADE CONFIG REQUIRED)

if (NOT OpenCASCADE_FOUND)
    message(FATAL_ERROR "OpenCASCADE not found")
endif()

add_executable(server main.cpp Server.cpp Server.h "MTQueue.hpp" "MainWindow.h" "OCCTViewer.h" "OCCTViewer.cpp" "MainWindow.cpp")
target_include_directories(server PRIVATE ${OPENCASCADE_INCLUDE_DIR})
target_link_libraries(server PRIVATE ${OpenCASCADE_LIBRARIES} Qt5::Widgets Boost::locale)
