#pragma once

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cstdio>

namespace mpm {

// CUDA error checking macro
#define CUDA_CHECK(call) \
    do { \
        cudaError_t error = call; \
        if (error != cudaSuccess) { \
            fprintf(stderr, "CUDA error at %s:%d - %s\n", \
                    __FILE__, __LINE__, cudaGetErrorString(error)); \
            exit(1); \
        } \
    } while(0)

// Kernel launch configuration
struct LaunchConfig {
    dim3 grid;
    dim3 block;
    
    static LaunchConfig for_1d(size_t n, int block_size = 256) {
        LaunchConfig config;
        config.block = dim3(block_size);
        config.grid = dim3((n + block_size - 1) / block_size);
        return config;
    }
};

// Device math utilities
template<typename T>
__device__ __forceinline__ T clamp(T x, T min, T max) {
    return x < min ? min : (x > max ? max : x);
}

// Quadratic B-spline kernel (as used in MPM)
// N(x) = 0.75 - x^2 for |x| < 0.5
// N(x) = 0.5 * (1.5 - |x|)^2 for 0.5 <= |x| < 1.5
// N(x) = 0 otherwise
__device__ __forceinline__ float quadratic_bspline(float x) {
    float abs_x = fabsf(x);
    if (abs_x < 0.5f) {
        return 0.75f - abs_x * abs_x;
    } else if (abs_x < 1.5f) {
        float t = 1.5f - abs_x;
        return 0.5f * t * t;
    }
    return 0.0f;
}

// Kernel gradient for APIC
__device__ __forceinline__ float quadratic_bspline_gradient(float x) {
    float abs_x = fabsf(x);
    float sign = (x > 0.0f) ? 1.0f : -1.0f;
    
    if (abs_x < 0.5f) {
        return -2.0f * x;
    } else if (abs_x < 1.5f) {
        return -sign * (1.5f - abs_x);
    }
    return 0.0f;
}

// 3D kernel weight (product of 1D kernels)
__device__ __forceinline__ float kernel_weight(const float3& dpos, float inv_dx) {
    float3 x = dpos * inv_dx;
    return quadratic_bspline(x.x) * quadratic_bspline(x.y) * quadratic_bspline(x.z);
}

// 3D kernel gradient
__device__ __forceinline__ float3 kernel_gradient(const float3& dpos, float inv_dx) {
    float3 x = dpos * inv_dx;
    float wx = quadratic_bspline(x.x);
    float wy = quadratic_bspline(x.y);
    float wz = quadratic_bspline(x.z);
    
    float gx = quadratic_bspline_gradient(x.x) * inv_dx;
    float gy = quadratic_bspline_gradient(x.y) * inv_dx;
    float gz = quadratic_bspline_gradient(x.z) * inv_dx;
    
    return make_float3(
        gx * wy * wz,
        wx * gy * wz,
        wx * wy * gz
    );
}

// Matrix operations for 3x3 matrices
__device__ __forceinline__ float3 mat3_mul_vec3(const float* mat, float3 v) {
    return make_float3(
        mat[0] * v.x + mat[1] * v.y + mat[2] * v.z,
        mat[3] * v.x + mat[4] * v.y + mat[5] * v.z,
        mat[6] * v.x + mat[7] * v.y + mat[8] * v.z
    );
}

__device__ __forceinline__ void mat3_transpose(const float* in, float* out) {
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out[i * 3 + j] = in[j * 3 + i];
        }
    }
}

__device__ __forceinline__ float mat3_determinant(const float* mat) {
    return mat[0] * (mat[4] * mat[8] - mat[5] * mat[7])
         - mat[1] * (mat[3] * mat[8] - mat[5] * mat[6])
         + mat[2] * (mat[3] * mat[7] - mat[4] * mat[6]);
}

// SVD (simplified version for 3x3)
// Returns U, S (singular values), Vt such that F = U * diag(S) * Vt
__device__ void svd3x3(const float* A, float* U, float* S, float* Vt);

} // namespace mpm
