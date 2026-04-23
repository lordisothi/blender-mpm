#include "mpm/material.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

namespace mpm {

void MaterialModel::compute_lame_parameters(Real E, Real nu, Real& mu, Real& lambda) const {
    mu = E / (2.0f * (1.0f + nu));
    lambda = E * nu / ((1.0f + nu) * (1.0f - 2.0f * nu));
}

// Snow material: Fixed corotated model with hardening
void SnowMaterial::compute_stress(
    const Mat3& F,
    const Mat3& F_elastic,
    Real mu,
    Real lambda,
    Mat3& stress
) const {
    // Use F_elastic for snow (plasticity tracked separately)
    const Mat3& F_e = F_elastic;
    
    // Compute SVD: F_e = U * S * V^T
    // For simplicity, using a basic polar decomposition approximation
    // In production, use proper SVD
    
    // J = det(F_e)
    Real J = glm::determinant(F_e);
    
    // R from polar decomposition (approximation)
    // F = R * S where R is rotation, S is stretch
    Mat3 F_e_T = glm::transpose(F_e);
    Mat3 S_sq = F_e_T * F_e;  // F^T * F = S^T * S
    
    // Fixed corotated model stress
    // P = 2 * mu * (F - R) + lambda * (J - 1) * J * F^{-T}
    
    // Cauchy stress: sigma = (1/J) * P * F^T
    // For MPM we use first Piola-Kirchhoff: P
    
    // Simplified version (no rotation extraction for now)
    Mat3 I(1.0f);
    Mat3 P = 2.0f * mu * (F_e - I) + lambda * (J - 1.0f) * J * glm::transpose(glm::inverse(F_e));
    
    // Cauchy stress
    stress = (1.0f / J) * P * glm::transpose(F_e);
}

Real SnowMaterial::compute_hardening(Real Jp, Real hardening_coeff) {
    return expf(hardening_coeff * (1.0f - Jp));
}

void SnowMaterial::snow_return_mapping(
    Mat3& F_elastic,
    Real Jp,
    Real critical_compression,
    Real critical_stretch
) {
    // SVD of F_elastic
    // F_elastic = U * diag(sigma) * V^T
    // Clamp singular values to [1 - critical_compression, 1 + critical_stretch]
    
    // Placeholder - proper SVD needed
    // For now, just ensure F_elastic is not too extreme
    Real J = glm::determinant(F_elastic);
    if (J < (1.0f - critical_compression)) {
        // Too compressed - scale up
        Real scale = powf((1.0f - critical_compression) / J, 1.0f / 3.0f);
        F_elastic = F_elastic * scale;
    } else if (J > (1.0f + critical_stretch)) {
        // Too stretched - scale down
        Real scale = powf((1.0f + critical_stretch) / J, 1.0f / 3.0f);
        F_elastic = F_elastic * scale;
    }
}

// Sand material: Drucker-Prager plasticity
void SandMaterial::compute_stress(
    const Mat3& F,
    const Mat3& F_elastic,
    Real mu,
    Real lambda,
    Mat3& stress
) const {
    const Mat3& F_e = F_elastic;
    Real J = glm::determinant(F_e);
    
    // Similar to snow but with different parameters
    Mat3 I(1.0f);
    Mat3 epsilon = 0.5f * (F_e + glm::transpose(F_e)) - I;  // Small strain
    
    // Cauchy stress for isotropic linear elasticity
    Real trace_epsilon = epsilon[0][0] + epsilon[1][1] + epsilon[2][2];
    stress = 2.0f * mu * epsilon + lambda * trace_epsilon * I;
}

void SandMaterial::sand_return_mapping(
    Mat3& F_elastic,
    Real& cohesion,
    Real friction_angle_deg
) {
    // Drucker-Prager return mapping
    // This is a simplified version - proper implementation requires SVD
    // and solving the yield condition
    
    Real friction_angle = friction_angle_deg * 3.14159265359f / 180.0f;
    Real sin_phi = sinf(friction_angle);
    
    // Yield function: f = ||s|| + (2 * I1 * sin_phi) / (3 - sin_phi) - cohesion
    // where s is deviatoric stress, I1 is first stress invariant
    
    // Placeholder - ensure determinant stays reasonable
    Real J = glm::determinant(F_elastic);
    if (J < 0.1f) {
        Real scale = powf(0.1f / J, 1.0f / 3.0f);
        F_elastic = F_elastic * scale;
    }
}

// Elastic material: Neo-Hookean
void ElasticMaterial::compute_stress(
    const Mat3& F,
    const Mat3& F_elastic,
    Real mu,
    Real lambda,
    Mat3& stress
) const {
    (void)F_elastic;  // Not used for elastic
    
    Real J = glm::determinant(F);
    Mat3 F_T = glm::transpose(F);
    Mat3 F_inv_T = glm::inverse(F_T);
    Mat3 I(1.0f);
    
    // First Piola-Kirchhoff stress for Neo-Hookean
    // P = mu * (F - F^{-T}) + lambda * ln(J) * F^{-T}
    Mat3 P = mu * (F - F_inv_T) + lambda * logf(J) * F_inv_T;
    
    // Cauchy stress: sigma = (1/J) * P * F^T
    stress = (1.0f / J) * P * F_T;
}

// Fluid material: Weakly compressible
void FluidMaterial::compute_stress(
    const Mat3& F,
    const Mat3& F_elastic,
    Real mu,
    Real lambda,
    Mat3& stress
) const {
    (void)F_elastic;
    (void)mu;  // For inviscid fluid
    (void)lambda;  // Using bulk modulus instead
    
    Real J = glm::determinant(F);
    
    // Pressure from equation of state
    Real pressure = compute_pressure(J, 2.0e5f);  // Default bulk modulus
    
    // Cauchy stress for fluid: sigma = -p * I
    stress = Mat3(-pressure);
}

Real FluidMaterial::compute_pressure(Real J, Real bulk_modulus) {
    // Tait equation or simple bulk modulus
    // p = bulk_modulus * (J - 1)
    return bulk_modulus * (J - 1.0f);
}

} // namespace mpm
