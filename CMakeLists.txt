cmake_minimum_required(VERSION 2.8)

project(pas CXX)

set(CXX_FLAGS -g -Wall)
set(CMAKE_CXX_COMPILER "g++")
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CXX_FLAGS}")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_compile_options(-std=c++11)

add_executable(TinyNet TinyNet.cc conn.cpp)
target_link_libraries(TinyNet pthread)