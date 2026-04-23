#pragma once

#include "mpm/types.h"
#include "mpm/material.h"
#include <memory>
#include <vector>

namespace mpm {

// Forward declarations
cudaStream_t get_stream(int i);

// Main MPM solver class
class MPMSolver {
public:
    MPMSolver();
    ~MPMSolver();
    
    // Disable copy
    MPMSolver(const MPMSolver&) = delete;
    MPMSolver& operator=(const MPMSolver&) = delete;
    
    // Configuration
    void set_params(const SimParams& params);
    void set_material_params(const MaterialParams& params);
    const SimParams& params() const { return params_; }
    const MaterialParams& material_params() const { return material_params_; }
    
    // Particle management
    void add_particles(const ParticleData& particles);
    void clear_particles();
    size_t particle_count() const;
    
    // Get/set particle data (for Python bindings)
    ParticleData get_particle_data() const;
    void set_particle_data(const ParticleData& data);
    
    // Simulation control
    void initialize();
    void step();
    void step_substep(Real dt);
    void advance_frame();
    
    // Grid management
    void update_grid_from_particles();
    void resize_grid();
    BBox compute_particle_bounds() const;
    
    // Caching
    void save_state(const std::string& filepath);
    void load_state(const std::string& filepath);
    
    // GPU data access (for visualization)
    void* get_gpu_positions() const;
    void* get_gpu_velocities() const;
    void download_particle_data(ParticleData& data) const;
    
private:
    // Parameters
    SimParams params_;
    MaterialParams material_params_;
    
    // Host data
    ParticleData particles_;
    
    // Device (GPU) data
    struct DeviceData;
    std::unique_ptr<DeviceData> device_;
    
    // Grid
    Grid grid_;
    
    // Materials
    std::unique_ptr<MaterialModel> snow_material_;
    std::unique_ptr<MaterialModel> sand_material_;
    std::unique_ptr<MaterialModel> elastic_material_;
    std::unique_ptr<MaterialModel> fluid_material_;
    
    // Internal methods
    void allocate_device_memory();
    void free_device_memory();
    void upload_to_device();
    void download_from_device();
    
    // MPM steps (GPU kernels)
    void p2g_transfer(Real dt);
    void grid_operations(Real dt);
    void g2p_transfer(Real dt);
    void update_deformation_gradient(Real dt);
    void apply_constitutive_model();
};

// GPU kernel declarations (implemented in .cu files)
void launch_p2g_kernel(
    const Vec3* positions,
    const Vec3* velocities,
    const Mat3* C,
    const Mat3* F,
    const Real* masses,
    const Real* volumes,
    const MaterialType* materials,
    const uint8_t* pinned,
    size_t num_particles,
    GridNode* grid,
    int grid_nx, int grid_ny, int grid_nz,
    Real grid_dx,
    Real dt,
    Real apic_factor,
    cudaStream_t stream
);

void launch_grid_velocity_update_kernel(
    GridNode* grid,
    int num_nodes,
    Real gravity,
    Real dt,
    int boundary_condition,
    Real boundary_friction,
    const Vec3& origin,
    Real dx,
    int nx, int ny, int nz,
    cudaStream_t stream
);

void launch_g2p_kernel(
    Vec3* positions,
    Vec3* velocities,
    Mat3* C,
    const GridNode* grid,
    size_t num_particles,
    int grid_nx, int grid_ny, int grid_nz,
    Real grid_dx,
    Real dt,
    Real apic_factor,
    const uint8_t* pinned,
    const Vec3* target_positions,
    cudaStream_t stream
);

void launch_update_F_kernel(
    Mat3* F,
    const Mat3* C,
    size_t num_particles,
    Real dt,
    cudaStream_t stream
);

} // namespace mpm
