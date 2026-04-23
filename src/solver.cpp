#include "mpm/solver.h"
#include "mpm/cuda_utils.h"
#include <cuda_runtime.h>
#include <cstdio>
#include <cstring>
#include <fstream>

namespace mpm {

// Device data structure (PIMPL pattern)
struct MPMSolver::DeviceData {
    // Particle data on GPU
    Vec3* d_positions = nullptr;
    Vec3* d_velocities = nullptr;
    Mat3* d_C = nullptr;
    Mat3* d_F = nullptr;
    Real* d_masses = nullptr;
    Real* d_volumes = nullptr;
    MaterialType* d_material_types = nullptr;
    uint8_t* d_pinned = nullptr;
    Vec3* d_target_positions = nullptr;
    
    // Material properties on GPU
    Real* d_mu = nullptr;
    Real* d_lambda = nullptr;
    Real* d_hardening = nullptr;
    Real* d_friction_angle = nullptr;
    Real* d_cohesion = nullptr;
    
    // Grid data on GPU
    GridNode* d_grid = nullptr;
    
    // Stress storage
    Mat3* d_stresses = nullptr;
    
    // Sizes
    size_t num_particles = 0;
    size_t num_grid_nodes = 0;
    size_t max_particles = 0;
    size_t max_grid_nodes = 0;
    
    // CUDA streams
    cudaStream_t stream = nullptr;
};

MPMSolver::MPMSolver() : device_(std::make_unique<DeviceData>()) {
    CUDA_CHECK(cudaStreamCreate(&device_->stream));
    
    // Initialize material models
    snow_material_ = std::make_unique<SnowMaterial>();
    sand_material_ = std::make_unique<SandMaterial>();
    elastic_material_ = std::make_unique<ElasticMaterial>();
    fluid_material_ = std::make_unique<FluidMaterial>();
}

MPMSolver::~MPMSolver() {
    free_device_memory();
    if (device_->stream) {
        cudaStreamDestroy(device_->stream);
    }
}

void MPMSolver::set_params(const SimParams& params) {
    params_ = params;
}

void MPMSolver::set_material_params(const MaterialParams& params) {
    material_params_ = params;
}

void MPMSolver::add_particles(const ParticleData& new_particles) {
    size_t start_idx = particles_.count();
    size_t new_count = new_particles.count();
    size_t total_count = start_idx + new_count;
    
    if (total_count > params_.max_particles) {
        fprintf(stderr, "Warning: Exceeding max particles limit\n");
        total_count = params_.max_particles;
        new_count = total_count - start_idx;
    }
    
    particles_.resize(total_count);
    
    // Copy new particles
    for (size_t i = 0; i < new_count; ++i) {
        particles_.position[start_idx + i] = new_particles.position[i];
        particles_.velocity[start_idx + i] = new_particles.velocity[i];
        particles_.C[start_idx + i] = new_particles.C[i];
        particles_.F[start_idx + i] = new_particles.F[i];
        particles_.mass[start_idx + i] = new_particles.mass[i];
        particles_.volume[start_idx + i] = new_particles.volume[i];
        particles_.volume0[start_idx + i] = new_particles.volume0[i];
        particles_.material_type[start_idx + i] = new_particles.material_type[i];
        particles_.mu[start_idx + i] = new_particles.mu[i];
        particles_.lambda[start_idx + i] = new_particles.lambda[i];
        particles_.hardening[start_idx + i] = new_particles.hardening[i];
        particles_.pinned[start_idx + i] = new_particles.pinned[i];
        particles_.target_position[start_idx + i] = new_particles.target_position[i];
        particles_.friction_angle[start_idx + i] = new_particles.friction_angle[i];
        particles_.cohesion[start_idx + i] = new_particles.cohesion[i];
    }
}

void MPMSolver::clear_particles() {
    particles_.clear();
    device_->num_particles = 0;
}

size_t MPMSolver::particle_count() const {
    return particles_.count();
}

ParticleData MPMSolver::get_particle_data() const {
    return particles_;
}

void MPMSolver::set_particle_data(const ParticleData& data) {
    particles_ = data;
}

void MPMSolver::allocate_device_memory() {
    free_device_memory();
    
    size_t max_p = params_.max_particles;
    size_t max_g = params_.max_grid_nodes;
    
    // Allocate particle data
    CUDA_CHECK(cudaMalloc(&device_->d_positions, max_p * sizeof(Vec3)));
    CUDA_CHECK(cudaMalloc(&device_->d_velocities, max_p * sizeof(Vec3)));
    CUDA_CHECK(cudaMalloc(&device_->d_C, max_p * sizeof(Mat3)));
    CUDA_CHECK(cudaMalloc(&device_->d_F, max_p * sizeof(Mat3)));
    CUDA_CHECK(cudaMalloc(&device_->d_masses, max_p * sizeof(Real)));
    CUDA_CHECK(cudaMalloc(&device_->d_volumes, max_p * sizeof(Real)));
    CUDA_CHECK(cudaMalloc(&device_->d_material_types, max_p * sizeof(MaterialType)));
    CUDA_CHECK(cudaMalloc(&device_->d_pinned, max_p * sizeof(uint8_t)));
    CUDA_CHECK(cudaMalloc(&device_->d_target_positions, max_p * sizeof(Vec3)));
    CUDA_CHECK(cudaMalloc(&device_->d_mu, max_p * sizeof(Real)));
    CUDA_CHECK(cudaMalloc(&device_->d_lambda, max_p * sizeof(Real)));
    CUDA_CHECK(cudaMalloc(&device_->d_hardening, max_p * sizeof(Real)));
    CUDA_CHECK(cudaMalloc(&device_->d_friction_angle, max_p * sizeof(Real)));
    CUDA_CHECK(cudaMalloc(&device_->d_cohesion, max_p * sizeof(Real)));
    
    // Allocate grid and stress data
    CUDA_CHECK(cudaMalloc(&device_->d_grid, max_g * sizeof(GridNode)));
    CUDA_CHECK(cudaMalloc(&device_->d_stresses, max_p * sizeof(Mat3)));
    
    device_->max_particles = max_p;
    device_->max_grid_nodes = max_g;
}

void MPMSolver::free_device_memory() {
    auto free_if_valid = [](void* ptr) {
        if (ptr) cudaFree(ptr);
    };
    
    free_if_valid(device_->d_positions);
    free_if_valid(device_->d_velocities);
    free_if_valid(device_->d_C);
    free_if_valid(device_->d_F);
    free_if_valid(device_->d_masses);
    free_if_valid(device_->d_volumes);
    free_if_valid(device_->d_material_types);
    free_if_valid(device_->d_pinned);
    free_if_valid(device_->d_target_positions);
    free_if_valid(device_->d_mu);
    free_if_valid(device_->d_lambda);
    free_if_valid(device_->d_hardening);
    free_if_valid(device_->d_friction_angle);
    free_if_valid(device_->d_cohesion);
    free_if_valid(device_->d_grid);
    free_if_valid(device_->d_stresses);
    
    device_->d_positions = nullptr;
    device_->d_velocities = nullptr;
    device_->d_C = nullptr;
    device_->d_F = nullptr;
    device_->d_masses = nullptr;
    device_->d_volumes = nullptr;
    device_->d_material_types = nullptr;
    device_->d_pinned = nullptr;
    device_->d_target_positions = nullptr;
    device_->d_mu = nullptr;
    device_->d_lambda = nullptr;
    device_->d_hardening = nullptr;
    device_->d_friction_angle = nullptr;
    device_->d_cohesion = nullptr;
    device_->d_grid = nullptr;
    device_->d_stresses = nullptr;
    
    device_->max_particles = 0;
    device_->max_grid_nodes = 0;
}

void MPMSolver::upload_to_device() {
    size_t n = particles_.count();
    if (n == 0 || n > device_->max_particles) return;
    
    device_->num_particles = n;
    
    CUDA_CHECK(cudaMemcpyAsync(device_->d_positions, particles_.position.data(),
                                 n * sizeof(Vec3), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_velocities, particles_.velocity.data(),
                                 n * sizeof(Vec3), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_C, particles_.C.data(),
                                 n * sizeof(Mat3), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_F, particles_.F.data(),
                                 n * sizeof(Mat3), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_masses, particles_.mass.data(),
                                 n * sizeof(Real), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_volumes, particles_.volume.data(),
                                 n * sizeof(Real), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_material_types, particles_.material_type.data(),
                                 n * sizeof(MaterialType), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_pinned, particles_.pinned.data(),
                                 n * sizeof(uint8_t), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_target_positions, particles_.target_position.data(),
                                 n * sizeof(Vec3), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_mu, particles_.mu.data(),
                                 n * sizeof(Real), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_lambda, particles_.lambda.data(),
                                 n * sizeof(Real), cudaMemcpyHostToDevice, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(device_->d_hardening, particles_.hardening.data(),
                                 n * sizeof(Real), cudaMemcpyHostToDevice, device_->stream));
}

void MPMSolver::download_from_device() {
    size_t n = device_->num_particles;
    if (n == 0) return;
    
    particles_.resize(n);
    
    CUDA_CHECK(cudaMemcpyAsync(particles_.position.data(), device_->d_positions,
                                 n * sizeof(Vec3), cudaMemcpyDeviceToHost, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(particles_.velocity.data(), device_->d_velocities,
                                 n * sizeof(Vec3), cudaMemcpyDeviceToHost, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(particles_.C.data(), device_->d_C,
                                 n * sizeof(Mat3), cudaMemcpyDeviceToHost, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(particles_.F.data(), device_->d_F,
                                 n * sizeof(Mat3), cudaMemcpyDeviceToHost, device_->stream));
    CUDA_CHECK(cudaMemcpyAsync(particles_.mass.data(), device_->d_masses,
                                 n * sizeof(Real), cudaMemcpyDeviceToHost, device_->stream));
    
    CUDA_CHECK(cudaStreamSynchronize(device_->stream));
}

void MPMSolver::initialize() {
    allocate_device_memory();
    resize_grid();
    upload_to_device();
}

BBox MPMSolver::compute_particle_bounds() const {
    if (particles_.count() == 0) {
        return BBox(Vec3(0), Vec3(1));
    }
    
    BBox bbox(particles_.position[0], particles_.position[0]);
    for (const auto& pos : particles_.position) {
        bbox.expand(pos);
    }
    return bbox;
}

void MPMSolver::resize_grid() {
    if (particles_.count() == 0) return;
    
    BBox bbox = compute_particle_bounds();
    Vec3 size = bbox.size();
    
    // Add padding
    Vec3 padding(params_.padding * params_.dx);
    bbox.min -= padding;
    bbox.max += padding;
    size = bbox.size();
    
    // Compute grid dimensions
    int nx = static_cast<int>(ceilf(size.x / params_.dx)) + 4;
    int ny = static_cast<int>(ceilf(size.y / params_.dx)) + 4;
    int nz = static_cast<int>(ceilf(size.z / params_.dx)) + 4;
    
    grid_.origin = bbox.min - Vec3(params_.dx * 2);
    grid_.dx = params_.dx;
    grid_.resize(nx, ny, nz);
    
    size_t num_nodes = static_cast<size_t>(nx) * ny * nz;
    if (num_nodes > params_.max_grid_nodes) {
        fprintf(stderr, "Warning: Grid exceeds max nodes, reducing resolution\n");
        // Reduce resolution proportionally
        float scale = powf(static_cast<float>(params_.max_grid_nodes) / num_nodes, 1.0f / 3.0f);
        nx = static_cast<int>(nx * scale);
        ny = static_cast<int>(ny * scale);
        nz = static_cast<int>(nz * scale);
        grid_.resize(nx, ny, nz);
    }
    
    if (device_->d_grid) {
        // Clear grid on device
        CUDA_CHECK(cudaMemsetAsync(device_->d_grid, 0, 
                                     grid_.nodes.size() * sizeof(GridNode),
                                     device_->stream));
    }
}

void MPMSolver::step() {
    if (device_->num_particles == 0) return;
    
    Real dt = params_.dt / params_.substeps;
    
    for (int step = 0; step < params_.substeps; ++step) {
        step_substep(dt);
    }
}

void MPMSolver::step_substep(Real dt) {
    // 1. Clear grid
    CUDA_CHECK(cudaMemsetAsync(device_->d_grid, 0,
                                 grid_.nodes.size() * sizeof(GridNode),
                                 device_->stream));
    
    // 2. P2G transfer
    p2g_transfer(dt);
    
    // 3. Grid operations (normalize, apply forces, boundary conditions)
    grid_operations(dt);
    
    // 4. G2P transfer
    g2p_transfer(dt);
    
    // 5. Update deformation gradient
    update_deformation_gradient(dt);
    
    // 6. Apply constitutive model (plasticity)
    apply_constitutive_model();
}

void MPMSolver::advance_frame() {
    step();
    download_from_device();
}

void MPMSolver::p2g_transfer(Real dt) {
    launch_p2g_kernel(
        device_->d_positions,
        device_->d_velocities,
        device_->d_C,
        device_->d_F,
        device_->d_masses,
        device_->d_volumes,
        device_->d_material_types,
        device_->d_pinned,
        device_->num_particles,
        device_->d_grid,
        grid_.nx, grid_.ny, grid_.nz,
        grid_.dx,
        dt,
        params_.apic_factor,
        device_->stream
    );
}

void MPMSolver::grid_operations(Real dt) {
    int num_nodes = grid_.nx * grid_.ny * grid_.nz;
    launch_grid_velocity_update_kernel(
        device_->d_grid,
        num_nodes,
        params_.gravity,
        dt,
        params_.boundary_condition,
        params_.boundary_friction,
        grid_.origin,
        grid_.dx,
        grid_.nx, grid_.ny, grid_.nz,
        device_->stream
    );
}

void MPMSolver::g2p_transfer(Real dt) {
    launch_g2p_kernel(
        device_->d_positions,
        device_->d_velocities,
        device_->d_C,
        device_->d_grid,
        device_->num_particles,
        grid_.nx, grid_.ny, grid_.nz,
        grid_.dx,
        dt,
        params_.apic_factor,
        device_->d_pinned,
        device_->d_target_positions,
        device_->stream
    );
}

void MPMSolver::update_deformation_gradient(Real dt) {
    launch_update_F_kernel(
        device_->d_F,
        device_->d_C,
        device_->num_particles,
        dt,
        device_->stream
    );
}

void MPMSolver::apply_constitutive_model() {
    // Launch material-specific stress kernels
    // This is a placeholder - in full implementation, each material
    // would have its own kernel for plasticity return mapping
}

void* MPMSolver::get_gpu_positions() const {
    return device_->d_positions;
}

void* MPMSolver::get_gpu_velocities() const {
    return device_->d_velocities;
}

void MPMSolver::download_particle_data(ParticleData& data) const {
    download_from_device();
    data = particles_;
}

void MPMSolver::save_state(const std::string& filepath) {
    download_from_device();
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        fprintf(stderr, "Failed to open %s for writing\n", filepath.c_str());
        return;
    }
    
    size_t n = particles_.count();
    file.write(reinterpret_cast<const char*>(&n), sizeof(n));
    
    file.write(reinterpret_cast<const char*>(particles_.position.data()), n * sizeof(Vec3));
    file.write(reinterpret_cast<const char*>(particles_.velocity.data()), n * sizeof(Vec3));
    file.write(reinterpret_cast<const char*>(particles_.F.data()), n * sizeof(Mat3));
    file.write(reinterpret_cast<const char*>(particles_.mass.data()), n * sizeof(Real));
    file.write(reinterpret_cast<const char*>(particles_.material_type.data()), n * sizeof(MaterialType));
    file.write(reinterpret_cast<const char*>(particles_.pinned.data()), n * sizeof(uint8_t));
    
    file.close();
}

void MPMSolver::load_state(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        fprintf(stderr, "Failed to open %s for reading\n", filepath.c_str());
        return;
    }
    
    size_t n;
    file.read(reinterpret_cast<char*>(&n), sizeof(n));
    
    particles_.resize(n);
    file.read(reinterpret_cast<char*>(particles_.position.data()), n * sizeof(Vec3));
    file.read(reinterpret_cast<char*>(particles_.velocity.data()), n * sizeof(Vec3));
    file.read(reinterpret_cast<char*>(particles_.F.data()), n * sizeof(Mat3));
    file.read(reinterpret_cast<char*>(particles_.mass.data()), n * sizeof(Real));
    file.read(reinterpret_cast<char*>(particles_.material_type.data()), n * sizeof(MaterialType));
    file.read(reinterpret_cast<char*>(particles_.pinned.data()), n * sizeof(uint8_t));
    
    file.close();
    
    upload_to_device();
}

} // namespace mpm
