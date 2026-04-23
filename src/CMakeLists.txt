# Core MPM library

set(CORE_SOURCES
    types.cpp
    material.cpp
    solver.cpp
)

set(CUDA_SOURCES
    cuda/kernels.cu
    cuda/svd.cu
    cuda/material_kernels.cu
)

# CUDA library
add_library(mpm_cuda STATIC ${CUDA_SOURCES})
set_target_properties(mpm_cuda PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
    CUDA_RESOLVE_DEVICE_SYMBOLS ON
)
target_link_libraries(mpm_cuda
    ${CUDA_LIBRARIES}
    ${CUDA_CUDART_LIBRARY}
)

# CPU library
add_library(mpm_core STATIC ${CORE_SOURCES})
target_link_libraries(mpm_core
    mpm_cuda
    glm::glm
)
