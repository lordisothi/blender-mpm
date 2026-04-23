#pragma once

#include <cuda_runtime.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

namespace mpm {

// Precision settings
typedef float Real;
typedef glm::vec<3, Real> Vec3;
typedef glm::vec<2, Real> Vec2;
typedef glm::mat<3, 3, Real> Mat3;
typedef glm::mat<2, 2, Real> Mat2;

// Material types
enum class MaterialType : uint8_t {
    SNOW = 0,
    SAND = 1,
    ELASTIC = 2,
    FLUID = 3
};

// Particle data structure (SoA layout for GPU efficiency)
struct ParticleData {
    // Position and velocity
    std::vector<Vec3> position;
    std::vector<Vec3> velocity;
    
    // Affine momentum (APIC)
    std::vector<Mat3> C;
    
    // Deformation gradient
    std::vector<Mat3> F;
    
    // Mass and volume
    std::vector<Real> mass;
    std::vector<Real> volume;
    std::vector<Real> volume0;  // Initial volume
    
    // Material properties
    std::vector<MaterialType> material_type;
    std::vector<Real> mu;      // Shear modulus
    std::vector<Real> lambda;  // Lame's first parameter
    std::vector<Real> hardening; // Hardening coefficient (snow)
    
    // Pinning (for animated/constrained particles)
    std::vector<uint8_t> pinned;
    std::vector<Vec3> target_position;
    
    // Additional material parameters
    std::vector<Real> friction_angle;  // For sand
    std::vector<Real> cohesion;        // For sand
    
    size_t count() const { return position.size(); }
    void resize(size_t n);
    void clear();
};

// Grid node data
struct GridNode {
    Vec3 velocity;
    Vec3 velocity_star;  // Temporary velocity
    Real mass;
    int active;  // Flag for active nodes (sparse grid)
};

// Grid structure
struct Grid {
    Vec3 origin;
    Real dx;
    int nx, ny, nz;
    
    // Flat array of grid nodes
    std::vector<GridNode> nodes;
    
    Grid() = default;
    Grid(const Vec3& origin, Real dx, int nx, int ny, int nz);
    
    void resize(int nx, int ny, int nz);
    void clear();
    size_t index(int i, int j, int k) const;
    Vec3 node_position(int i, int j, int k) const;
};

// Simulation parameters
struct SimParams {
    Real dt = 1.0f / 60.0f;           // Timestep
    int substeps = 20;                 // Substeps per frame
    Real gravity = -9.8f;              // Gravity (m/s^2)
    Real dx = 0.02f;                   // Grid spacing
    int max_particles = 1000000;       // Maximum particles
    int max_grid_nodes = 10000000;     // Maximum grid nodes
    Real padding = 4;                  // Grid padding around particles
    
    // APIC/PIC blend (0 = PIC, 1 = APIC)
    Real apic_factor = 1.0f;
    
    // Boundary conditions
    Real boundary_friction = 0.0f;     // Wall friction
    int boundary_condition = 1;        // 0 = sticky, 1 = separate, 2 = slip
};

// Material parameters
struct MaterialParams {
    // Snow parameters (from Stomakhin et al.)
    Real snow_density = 400.0f;
    Real snow_E = 1.4e5f;              // Young's modulus
    Real snow_nu = 0.2f;               // Poisson ratio
    Real snow_hardening = 10.0f;
    Real snow_critical_compression = 2.5e-2f;
    Real snow_critical_stretch = 7.5e-3f;
    
    // Sand parameters
    Real sand_density = 1600.0f;
    Real sand_E = 3.5e5f;
    Real sand_nu = 0.3f;
    Real sand_friction_angle = 35.0f;  // degrees
    
    // Elastic parameters
    Real elastic_density = 1000.0f;
    Real elastic_E = 1.0e4f;
    Real elastic_nu = 0.3f;
    
    // Fluid parameters
    Real fluid_density = 1000.0f;
    Real fluid_bulk_modulus = 2.0e5f;
    Real fluid_viscosity = 0.01f;
};

// Bounding box
struct BBox {
    Vec3 min;
    Vec3 max;
    
    BBox() : min(Vec3(0)), max(Vec3(0)) {}
    BBox(const Vec3& min, const Vec3& max) : min(min), max(max) {}
    
    Vec3 center() const { return (min + max) * 0.5f; }
    Vec3 size() const { return max - min; }
    void expand(const Vec3& p);
    void expand(const BBox& other);
};

} // namespace mpm
