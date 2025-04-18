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
find_package(fmt CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

set(PYBIND11_FINDPYTHON ON)
pybind11_add_module(Server MODULE "Server.cpp")

target_link_libraries(Server PRIVATE pybind11::module)
target_link_libraries(Server PRIVATE magic_enum::magic_enum)
target_link_libraries(Server PRIVATE fmt::fmt-header-only)
target_link_libraries(Server PRIVATE nlohmann_json::nlohmann_json)

set(MODULE_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/notebooks/python_modules")
set(MODULE_FILE "${MODULE_OUTPUT_DIR}/Server.pyd")

add_custom_command(TARGET Server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MODULE_OUTPUT_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:Server>
        "${MODULE_FILE}"
)

add_custom_command(TARGET Server POST_BUILD
    COMMAND python -m pybind11_stubgen Server --output-dir "${MODULE_OUTPUT_DIR}"
    WORKING_DIRECTORY "${MODULE_OUTPUT_DIR}"
    COMMENT "Generating .pyi stub file using pybind11-stubgen"
)


