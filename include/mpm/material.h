#pragma once

#include "mpm/types.h"
#include <cuda_runtime.h>

namespace mpm {

// Base class for material models
class MaterialModel {
public:
    virtual ~MaterialModel() = default;
    
    // Compute stress from deformation gradient
    // Output: stress (Cauchy stress, will be converted to first Piola-Kirchhoff in solver)
    virtual void compute_stress(
        const Mat3& F,
        const Mat3& F_elastic,  // For plastic materials (snow, sand)
        Real mu,
        Real lambda,
        Mat3& stress
    ) const = 0;
    
    // GPU kernel launch for this material
    virtual void launch_stress_kernel(
        const Mat3* F,
        const Mat3* F_elastic,
        const Real* mu,
        const Real* lambda,
        const MaterialType* types,
        Mat3* stresses,
        size_t num_particles,
        const MaterialParams& params,
        cudaStream_t stream
    ) const = 0;
    
    // Material-specific parameters
    virtual Real get_default_density() const = 0;
    virtual void compute_lame_parameters(Real E, Real nu, Real& mu, Real& lambda) const;
};

// Snow material (Stomakhin et al. "A material point method for snow simulation")
class SnowMaterial : public MaterialModel {
public:
    void compute_stress(
        const Mat3& F,
        const Mat3& F_elastic,
        Real mu,
        Real lambda,
        Mat3& stress
    ) const override;
    
    void launch_stress_kernel(
        const Mat3* F,
        const Mat3* F_elastic,
        const Real* mu,
        const Real* lambda,
        const MaterialType* types,
        Mat3* stresses,
        size_t num_particles,
        const MaterialParams& params,
        cudaStream_t stream
    ) const override;
    
    Real get_default_density() const override { return 400.0f; }
    
    // Snow-specific: compute hardening factor
    static Real compute_hardening(Real Jp, Real hardening_coeff);
    
    // Plasticity return mapping for snow
    static void snow_return_mapping(Mat3& F_elastic, Real Jp, 
                                     Real critical_compression,
                                     Real critical_stretch);
};

// Sand material (Klar et al. "Sand simulation with Material Point Method")
class SandMaterial : public MaterialModel {
public:
    void compute_stress(
        const Mat3& F,
        const Mat3& F_elastic,
        Real mu,
        Real lambda,
        Mat3& stress
    ) const override;
    
    void launch_stress_kernel(
        const Mat3* F,
        const Mat3* F_elastic,
        const Real* mu,
        const Real* lambda,
        const MaterialType* types,
        Mat3* stresses,
        size_t num_particles,
        const MaterialParams& params,
        cudaStream_t stream
    ) const override;
    
    Real get_default_density() const override { return 1600.0f; }
    
    // Drucker-Prager return mapping
    static void sand_return_mapping(Mat3& F_elastic, Real& cohesion,
                                   Real friction_angle_deg);
};

// Elastic material (Neo-Hookean)
class ElasticMaterial : public MaterialModel {
public:
    void compute_stress(
        const Mat3& F,
        const Mat3& F_elastic,
        Real mu,
    Real lambda,
        Mat3& stress
    ) const override;
    
    void launch_stress_kernel(
        const Mat3* F,
        const Mat3* F_elastic,
        const Real* mu,
        const Real* lambda,
        const MaterialType* types,
        Mat3* stresses,
        size_t num_particles,
        const MaterialParams& params,
        cudaStream_t stream
    ) const override;
    
    Real get_default_density() const override { return 1000.0f; }
};

// Fluid material (Weakly compressible)
class FluidMaterial : public MaterialModel {
public:
    void compute_stress(
        const Mat3& F,
        const Mat3& F_elastic,
        Real mu,
        Real lambda,
        Mat3& stress
    ) const override;
    
    void launch_stress_kernel(
        const Mat3* F,
        const Mat3* F_elastic,
        const Real* mu,
        const Real* lambda,
        const MaterialType* types,
        Mat3* stresses,
        size_t num_particles,
        const MaterialParams& params,
        cudaStream_t stream
    ) const override;
    
    Real get_default_density() const override { return 1000.0f; }
    
    // Equation of state for pressure
    static Real compute_pressure(Real J, Real bulk_modulus);
};

} // namespace mpm
