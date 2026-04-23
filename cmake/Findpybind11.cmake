# Find pybind11
# This module finds pybind11 installation

find_package(Python COMPONENTS Interpreter Development REQUIRED)

# Try to find pybind11 using Python
execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import pybind11; print(pybind11.get_cmake_dir())"
    OUTPUT_VARIABLE PYBIND11_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(PYBIND11_CMAKE_DIR)
    list(APPEND CMAKE_PREFIX_PATH ${PYBIND11_CMAKE_DIR})
    find_package(pybind11 CONFIG REQUIRED)
else()
    # Fallback: try standard paths
    find_path(PYBIND11_INCLUDE_DIR pybind11/pybind11.h
        PATHS
            ${Python_INCLUDE_DIRS}
            /usr/include
            /usr/local/include
            ${CMAKE_PREFIX_PATH}
        PATH_SUFFIXES pybind11
    )
    
    if(PYBIND11_INCLUDE_DIR)
        add_library(pybind11::headers INTERFACE IMPORTED)
        set_target_properties(pybind11::headers PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES ${PYBIND11_INCLUDE_DIR}
        )
        set(pybind11_FOUND TRUE)
    endif()
endif()

if(NOT pybind11_FOUND)
    message(FATAL_ERROR "pybind11 not found. Install with: pip install pybind11")
endif()
