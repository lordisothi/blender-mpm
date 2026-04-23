# Python bindings

pybind11_add_module(pympm
    bindings.cpp
)

target_link_libraries(pympm PRIVATE
    mpm_core
    mpm_cuda
    ${CUDA_LIBRARIES}
)

set_target_properties(pympm PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
