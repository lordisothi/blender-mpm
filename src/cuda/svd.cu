#include "mpm/cuda_utils.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <cmath>

namespace mpm {

// Simplified 3x3 SVD for MPM constitutive models
// Based on the method by McAdams et al. "Computing the Singular Value Decomposition of 3x3 matrices"
// This is a GPU-friendly implementation

__device__ void svd3x3(const float* A, float* U, float* S, float* Vt) {
    // Compute A^T * A for the right singular vectors
    float ATA[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            ATA[i*3+j] = 0;
            for (int k = 0; k < 3; ++k) {
                ATA[i*3+j] += A[k*3+i] * A[k*3+j];
            }
        }
    }
    
    // Symmetric eigenvalue decomposition of ATA
    // Using Jacobi method for simplicity
    float eigenvectors[9] = {1,0,0, 0,1,0, 0,0,1};
    float eigenvalues[3];
    
    // Jacobi iterations
    const int max_iterations = 10;
    for (int iter = 0; iter < max_iterations; ++iter) {
        // Find largest off-diagonal element
        float max_val = 0.0f;
        int p = 0, q = 1;
        
        for (int i = 0; i < 3; ++i) {
            for (int j = i+1; j < 3; ++j) {
                float val = fabsf(ATA[i*3+j]);
                if (val > max_val) {
                    max_val = val;
                    p = i;
                    q = j;
                }
            }
        }
        
        if (max_val < 1e-6f) break;
        
        // Compute Jacobi rotation
        float phi = 0.5f * atan2f(2.0f * ATA[p*3+q], ATA[q*3+q] - ATA[p*3+p]);
        float c = cosf(phi);
        float s = sinf(phi);
        
        // Update ATA
        float app = ATA[p*3+p];
        float aqq = ATA[q*3+q];
        float apq = ATA[p*3+q];
        
        ATA[p*3+p] = c*c*app - 2.0f*c*s*apq + s*s*aqq;
        ATA[q*3+q] = s*s*app + 2.0f*c*s*apq + c*c*aqq;
        ATA[p*3+q] = ATA[q*3+p] = 0.0f;
        
        for (int k = 0; k < 3; ++k) {
            if (k != p && k != q) {
                float akp = ATA[k*3+p];
                float akq = ATA[k*3+q];
                ATA[k*3+p] = ATA[p*3+k] = c*akp - s*akq;
                ATA[k*3+q] = ATA[q*3+k] = s*akp + c*akq;
            }
        }
        
        // Update eigenvectors
        for (int k = 0; k < 3; ++k) {
            float ekp = eigenvectors[k*3+p];
            float ekq = eigenvectors[k*3+q];
            eigenvectors[k*3+p] = c*ekp - s*ekq;
            eigenvectors[k*3+q] = s*ekp + c*ekq;
        }
    }
    
    // Extract eigenvalues (singular values squared)
    eigenvalues[0] = sqrtf(fmaxf(ATA[0], 0.0f));
    eigenvalues[1] = sqrtf(fmaxf(ATA[4], 0.0f));
    eigenvalues[2] = sqrtf(fmaxf(ATA[8], 0.0f));
    
    // Sort singular values in descending order
    for (int i = 0; i < 2; ++i) {
        for (int j = i+1; j < 3; ++j) {
            if (eigenvalues[i] < eigenvalues[j]) {
                float tmp = eigenvalues[i];
                eigenvalues[i] = eigenvalues[j];
                eigenvalues[j] = tmp;
                
                // Swap columns in eigenvectors
                for (int k = 0; k < 3; ++k) {
                    float tmp_v = eigenvectors[k*3+i];
                    eigenvectors[k*3+i] = eigenvectors[k*3+j];
                    eigenvectors[k*3+j] = tmp_v;
                }
            }
        }
    }
    
    // Vt = eigenvectors^T
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Vt[i*3+j] = eigenvectors[j*3+i];
        }
    }
    
    // Compute U = A * V * inv(S)
    float AV[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            AV[i*3+j] = 0;
            for (int k = 0; k < 3; ++k) {
                AV[i*3+j] += A[i*3+k] * eigenvectors[k*3+j];
            }
        }
    }
    
    // Divide by singular values
    float eps = 1e-10f;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            U[i*3+j] = AV[i*3+j] / fmaxf(eigenvalues[j], eps);
        }
    }
    
    // Set singular values
    S[0] = eigenvalues[0];
    S[1] = eigenvalues[1];
    S[2] = eigenvalues[2];
}

// Polar decomposition using SVD: F = R * S where R is rotation, S is symmetric
__device__ void polar_decomposition(const float* F, float* R, float* S) {
    float U[9], sig[3], Vt[9];
    svd3x3(F, U, sig, Vt);
    
    // R = U * Vt
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i*3+j] = 0;
            for (int k = 0; k < 3; ++k) {
                R[i*3+j] += U[i*3+k] * Vt[k*3+j];
            }
        }
    }
    
    // S = V * Sigma * Vt
    float V[9];
    mat3_transpose(Vt, V);
    
    float VS[9];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            VS[i*3+j] = V[i*3+j] * sig[j];
        }
    }
    
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            S[i*3+j] = 0;
            for (int k = 0; k < 3; ++k) {
                S[i*3+j] += VS[i*3+k] * Vt[k*3+j];
            }
        }
    }
}

// Clamp singular values for plasticity
__device__ void clamp_singular_values(float* S, float min_val, float max_val) {
    S[0] = clamp(S[0], min_val, max_val);
    S[1] = clamp(S[1], min_val, max_val);
    S[2] = clamp(S[2], min_val, max_val);
}

} // namespace mpm
