#include "mpm/cuda_utils.h"
#include "mpm/types.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>

namespace mpm {

// Snow plasticity return mapping
__global__ void snow_return_mapping_kernel(
    Mat3* F,
    Real* Jp,
    size_t num_particles,
    Real critical_compression,
    Real critical_stretch
) {
    int pid = blockIdx.x * blockDim.x + threadIdx.x;
    if (pid >= num_particles) return;
    
    // Get deformation gradient
    const Mat3& f = F[pid];
    Real J = glm::determinant(f);
    
    // F = F_elastic * F_plastic
    // We track F_plastic implicitly through volume change (Jp)
    
    // Clamp volume change
    Real J_min = (1.0f - critical_compression);
    Real J_max = (1.0f + critical_stretch);
    
    if (J < J_min || J > J_max) {
        // Apply plastic return mapping
        Real J_clamped = fmaxf(fminf(J, J_max), J_min);
        Real scale = powf(J_clamped / J, 1.0f / 3.0f);
        
        // Scale F to bring it back to valid range
        F[pid] = f * scale;
        
        // Update plastic deformation tracking
        Jp[pid] *= J / J_clamped;
    }
}

// Sand plasticity (Drucker-Prager yield criterion)
__global__ void sand_return_mapping_kernel(
    Mat3* F,
    const MaterialType* types,
    size_t num_particles,
    Real friction_angle_deg
) {
    int pid = blockIdx.x * blockDim.x + threadIdx.x;
    if (pid >= num_particles) return;
    
    // Only process sand particles
    if (types[pid] != MaterialType::SAND) return;
    
    // Convert friction angle
    Real phi = friction_angle_deg * 3.14159265359f / 180.0f;
    Real sin_phi = sinf(phi);
    
    // Drucker-Prager parameters
    Real alpha = sqrtf(2.0f / 3.0f) * 2.0f * sin_phi / (3.0f - sin_phi);
    Real kc = 0.0f;  // Cohesion (simplified)
    
    // Get F and compute stress
    const Mat3& f = F[pid];
    Real J = glm::determinant(f);
    
    // Simplified: ensure J doesn't get too small (prevent inversion)
    if (J < 0.5f) {
        Real scale = powf(0.5f / J, 1.0f / 3.0f);
        F[pid] = f * scale;
    }
    
    // Full Drucker-Prager would require SVD and solving yield condition
    // This is the simplified version for stability
}

// Compute stress for all materials
__global__ void compute_stress_kernel(
    const Mat3* F,
    const Real* mu,
    const Real* lambda,
    const MaterialType* types,
    const Real* hardening,
    Mat3* stresses,
    size_t num_particles
) {
    int pid = blockIdx.x * blockDim.x + threadIdx.x;
    if (pid >= num_particles) return;
    
    const Mat3& f = F[pid];
    Real mu_p = mu[pid];
    Real lambda_p = lambda[pid];
    MaterialType type = types[pid];
    
    Real J = glm::determinant(f);
    Mat3 f_T = glm::transpose(f);
    Mat3 f_inv_T = glm::inverse(f_T);
    Mat3 I(1.0f);
    
    Mat3 stress(0.0f);
    
    switch (type) {
        case MaterialType::SNOW: {
            // Fixed corotated with hardening
            Real hard = hardening[pid];
            mu_p *= hard;
            lambda_p *= hard;
            
            // P = 2 * mu * (F - R) + lambda * (J - 1) * J * F^{-T}
            // Simplified: use F instead of R (proper SVD needed)
            Mat3 P = 2.0f * mu_p * (f - I) + lambda_p * (J - 1.0f) * J * f_inv_T;
            stress = (1.0f / J) * P * f_T;
            break;
        }
        
        case MaterialType::SAND: {
            // Elasticity with Drucker-Prager (simplified)
            // Using Neo-Hookean as base
            Mat3 P = mu_p * (f - f_inv_T) + lambda_p * logf(J) * f_inv_T;
            stress = (1.0f / J) * P * f_T;
            break;
        }
        
        case MaterialType::ELASTIC: {
            // Neo-Hookean
            Mat3 P = mu_p * (f - f_inv_T) + lambda_p * logf(J) * f_inv_T;
            stress = (1.0f / J) * P * f_T;
            break;
        }
        
        case MaterialType::FLUID: {
            // Weakly compressible fluid
            // P = -bulk_modulus * (J - 1) * I
            Real bulk_modulus = lambda_p;  // Using lambda as bulk modulus for fluids
            Real pressure = -bulk_modulus * (J - 1.0f);
            stress = Mat3(pressure);
            break;
        }
    }
    
    stresses[pid] = stress;
}

// Launch wrapper for stress computation
void launch_stress_computation(
    const Mat3* F,
    const Real* mu,
    const Real* lambda,
    const MaterialType* types,
    const Real* hardening,
    Mat3* stresses,
    size_t num_particles,
    cudaStream_t stream
) {
    LaunchConfig config = LaunchConfig::for_1d(num_particles, 256);
    compute_stress_kernel<<<config.grid, config.block, 0, stream>>>(
        F, mu, lambda, types, hardening, stresses, num_particles
    );
    CUDA_CHECK(cudaGetLastError());
}

// Launch wrapper for snow return mapping
void launch_snow_return_mapping(
    Mat3* F,
    Real* Jp,
    size_t num_particles,
    Real critical_compression,
    Real critical_stretch,
    cudaStream_t stream
) {
    LaunchConfig config = LaunchConfig::for_1d(num_particles, 256);
    snow_return_mapping_kernel<<<config.grid, config.block, 0, stream>>>(
        F, Jp, num_particles, critical_compression, critical_stretch
    );
    CUDA_CHECK(cudaGetLastError());
}

// Launch wrapper for sand return mapping
void launch_sand_return_mapping(
    Mat3* F,
    const MaterialType* types,
    size_t num_particles,
    Real friction_angle_deg,
    cudaStream_t stream
) {
    LaunchConfig config = LaunchConfig::for_1d(num_particles, 256);
    sand_return_mapping_kernel<<<config.grid, config.block, 0, stream>>>(
        F, types, num_particles, friction_angle_deg
    );
    CUDA_CHECK(cudaGetLastError());
}

} // namespace mpm
