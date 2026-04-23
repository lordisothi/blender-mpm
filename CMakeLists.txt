cmake_minimum_required(VERSION 3.20)
project(blender_mpm VERSION 1.0.0 LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Find packages
find_package(CUDA REQUIRED)
find_package(Python COMPONENTS Development REQUIRED)
find_package(pybind11 REQUIRED)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_PYTHON_BINDINGS "Build Python bindings" ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CUDA_INCLUDE_DIRS}
)

# Core library
add_subdirectory(src)

# Python bindings
if(BUILD_PYTHON_BINDINGS)
    add_subdirectory(bindings)
endif()

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
