# CMakeList.txt : CMake project for Server, include source and define
# project specific logic here.
#

cmake_minimum_required(VERSION 3.15...3.22)
project(Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

find_package(magic_enum CONFIG REQUIRED)

pybind11_add_module(Server MODULE "Server.cpp")

target_link_libraries(Server PRIVATE pybind11::module)
target_link_libraries(Server PRIVATE magic_enum::magic_enum)
