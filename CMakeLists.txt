cmake_minimum_required(VERSION 3.12)

project(TestOcct LANGUAGES CXX)

include(cmake/compile_cxx17.cmake)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE Release)

find_package(Boost REQUIRED COMPONENTS locale)
find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(OpenCASCADE CONFIG REQUIRED)

if (NOT OpenCASCADE_FOUND)
    message(FATAL_ERROR "OpenCASCADE not found")
endif()

add_executable(server 
"src/server/main.cpp" 
"src/server/Server.cpp" 
"src/server/OcctViewer.cpp" 
"src/server/MainWindow.cpp"
"include/server/Server.h" 
"include/common/MTQueue.hpp" 
"include/server/MainWindow.h" 
"include/server/OcctViewer.h")
target_include_directories(server PRIVATE include ${OPENCASCADE_INCLUDE_DIR})
target_link_libraries(server PRIVATE ${OpenCASCADE_LIBRARIES} Qt5::Widgets Boost::locale)
set_target_properties(server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/server)

add_executable(client "src/client/Client.cpp" "include/client/RemoteDebugTools.hpp")
target_include_directories(client PRIVATE include ${OPENCASCADE_INCLUDE_DIR})
target_link_libraries(client PRIVATE ${OpenCASCADE_LIBRARIES} Boost::locale)
set_target_properties(client PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/client)