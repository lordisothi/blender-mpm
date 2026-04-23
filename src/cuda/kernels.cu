#include "mpm/cuda_utils.h"
#include "mpm/types.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <glm/glm.hpp>

namespace mpm {

// Convert glm types to CUDA float types
__device__ __forceinline__ float3 to_float3(const Vec3& v) {
    return make_float3(v.x, v.y, v.z);
}

__device__ __forceinline__ Vec3 to_vec3(const float3& v) {
    return Vec3(v.x, v.y, v.z);
}

// P2G transfer kernel
__global__ void p2g_kernel(
    const Vec3* positions,
    const Vec3* velocities,
    const Mat3* C,
    const Mat3* F,
    const Real* masses,
    const Real* volumes,
    const MaterialType* material_types,
    const uint8_t* pinned,
    size_t num_particles,
    GridNode* grid,
    int grid_nx, int grid_ny, int grid_nz,
    Real grid_dx,
    Real dt,
    Real apic_factor
) {
    int pid = blockIdx.x * blockDim.x + threadIdx.x;
    if (pid >= num_particles) return;
    
    // Skip pinned particles for mass transfer (they don't contribute to grid dynamics)
    // But they still need to be tracked
    
    float3 pos = to_float3(positions[pid]);
    float3 vel = to_float3(velocities[pid]);
    Real mass = masses[pid];
    Real volume = volumes[pid];
    
    // Get base grid coordinate
    int base_i = static_cast<int>(floorf((pos.x - grid_dx * 2) / grid_dx));
    int base_j = static_cast<int>(floorf((pos.y - grid_dx * 2) / grid_dx));
    int base_k = static_cast<int>(floorf((pos.z - grid_dx * 2) / grid_dx));
    
    Real inv_dx = 1.0f / grid_dx;
    
    // Transfer to 3x3x3 neighborhood
    for (int gi = 0; gi < 3; ++gi) {
        for (int gj = 0; gj < 3; ++gj) {
            for (int gk = 0; gk < 3; ++gk) {
                int i = base_i + gi;
                int j = base_j + gj;
                int k = base_k + gk;
                
                if (i < 0 || i >= grid_nx || j < 0 || j >= grid_ny || k < 0 || k >= grid_nz) {
                    continue;
                }
                
                float3 node_pos = make_float3(
                    (i + 0.5f) * grid_dx,
                    (j + 0.5f) * grid_dx,
                    (k + 0.5f) * grid_dx
                );
                
                float3 dpos = pos - node_pos;
                float weight = kernel_weight(dpos, inv_dx);
                
                if (weight < 1e-6f) continue;
                
                // Momentum transfer: m * v * w
                // APIC adds: m * C * (x_p - x_i) * w
                float3 momentum = vel * mass * weight;
                
                if (apic_factor > 0.0f) {
                    // Convert C matrix to float array for device math
                    const Mat3& c_mat = C[pid];
                    float c[9] = {
                        c_mat[0][0], c_mat[1][0], c_mat[2][0],
                        c_mat[0][1], c_mat[1][1], c_mat[2][1],
                        c_mat[0][2], c_mat[1][2], c_mat[2][2]
                    };
                    float3 apic_term = mat3_mul_vec3(c, dpos);
                    momentum.x += mass * apic_term.x * weight * apic_factor;
                    momentum.y += mass * apic_term.y * weight * apic_factor;
                    momentum.z += mass * apic_term.z * weight * apic_factor;
                }
                
                size_t gid = static_cast<size_t>(k) * grid_nx * grid_ny + 
                            static_cast<size_t>(j) * grid_nx + i;
                
                // Atomic add to grid
                atomicAdd(&grid[gid].mass, mass * weight);
                atomicAdd(&grid[gid].velocity.x, momentum.x);
                atomicAdd(&grid[gid].velocity.y, momentum.y);
                atomicAdd(&grid[gid].velocity.z, momentum.z);
                atomicOr(&grid[gid].active, 1);
            }
        }
    }
}

void launch_p2g_kernel(
    const Vec3* positions,
    const Vec3* velocities,
    const Mat3* C,
    const Mat3* F,
    const Real* masses,
    const Real* volumes,
    const MaterialType* material_types,
    const uint8_t* pinned,
    size_t num_particles,
    GridNode* grid,
    int grid_nx, int grid_ny, int grid_nz,
    Real grid_dx,
    Real dt,
    Real apic_factor,
    cudaStream_t stream
) {
    LaunchConfig config = LaunchConfig::for_1d(num_particles, 256);
    p2g_kernel<<<config.grid, config.block, 0, stream>>>(
        positions, velocities, C, F, masses, volumes, material_types, pinned,
        num_particles, grid, grid_nx, grid_ny, grid_nz, grid_dx,
        dt, apic_factor
    );
    CUDA_CHECK(cudaGetLastError());
}

// Grid velocity update kernel
__global__ void grid_velocity_update_kernel(
    GridNode* grid,
    int num_nodes,
    Real gravity,
    Real dt,
    int boundary_condition,
    Real boundary_friction,
    Vec3 origin,
    Real dx,
    int nx, int ny, int nz
) {
    int gid = blockIdx.x * blockDim.x + threadIdx.x;
    if (gid >= num_nodes) return;
    
    GridNode& node = grid[gid];
    
    if (node.mass < 1e-10f) {
        node.velocity = Vec3(0);
        node.active = 0;
        return;
    }
    
    // Normalize velocity by mass
    node.velocity /= node.mass;
    
    // Store pre-velocity for boundary handling
    Vec3 v_before = node.velocity;
    
    // Apply gravity
    node.velocity.y += gravity * dt;
    
    // Compute grid coordinates
    int k = gid / (nx * ny);
    int j = (gid % (nx * ny)) / nx;
    int i = gid % nx;
    
    // Boundary conditions
    bool at_boundary = (i < 2 || i >= nx - 2 || j < 2 || j >= ny - 2 || k < 2 || k >= nz - 2);
    
    if (at_boundary) {
        if (boundary_condition == 0) {
            // Sticky: zero velocity
            node.velocity = Vec3(0);
        } else if (boundary_condition == 1) {
            // Separate: prevent inward motion
            if (i < 2) node.velocity.x = fmaxf(node.velocity.x, 0);
            if (i >= nx - 2) node.velocity.x = fminf(node.velocity.x, 0);
            if (j < 2) node.velocity.y = fmaxf(node.velocity.y, 0);
            if (j >= ny - 2) node.velocity.y = fminf(node.velocity.y, 0);
            if (k < 2) node.velocity.z = fmaxf(node.velocity.z, 0);
            if (k >= nz - 2) node.velocity.z = fminf(node.velocity.z, 0);
        }
        // Slip is default (free boundary)
    }
    
    node.velocity_star = node.velocity;
}

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
) {
    LaunchConfig config = LaunchConfig::for_1d(num_nodes, 256);
    grid_velocity_update_kernel<<<config.grid, config.block, 0, stream>>>(
        grid, num_nodes, gravity, dt, boundary_condition, boundary_friction,
        origin, dx, nx, ny, nz
    );
    CUDA_CHECK(cudaGetLastError());
}

// G2P transfer kernel
__global__ void g2p_kernel(
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
    const Vec3* target_positions
) {
    int pid = blockIdx.x * blockDim.x + threadIdx.x;
    if (pid >= num_particles) return;
    
    // Handle pinned particles
    if (pinned && pinned[pid]) {
        // Pinned particle: keep position (optionally move to target)
        if (target_positions) {
            positions[pid] = target_positions[pid];
        }
        // Velocity is maintained by user or set to zero
        velocities[pid] = Vec3(0);
        C[pid] = Mat3(0);
        return;
    }
    
    float3 pos = to_float3(positions[pid]);
    float3 vel = make_float3(0, 0, 0);
    
    // Affine velocity gradient
    float c[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
    
    int base_i = static_cast<int>(floorf((pos.x - grid_dx * 2) / grid_dx));
    int base_j = static_cast<int>(floorf((pos.y - grid_dx * 2) / grid_dx));
    int base_k = static_cast<int>(floorf((pos.z - grid_dx * 2) / grid_dx));
    
    Real inv_dx = 1.0f / grid_dx;
    
    // Gather from 3x3x3 neighborhood
    for (int gi = 0; gi < 3; ++gi) {
        for (int gj = 0; gj < 3; ++gj) {
            for (int gk = 0; gk < 3; ++gk) {
                int i = base_i + gi;
                int j = base_j + gj;
                int k = base_k + gk;
                
                if (i < 0 || i >= grid_nx || j < 0 || j >= grid_ny || k < 0 || k >= grid_nz) {
                    continue;
                }
                
                float3 node_pos = make_float3(
                    (i + 0.5f) * grid_dx,
                    (j + 0.5f) * grid_dx,
                    (k + 0.5f) * grid_dx
                );
                
                float3 dpos = pos - node_pos;
                float weight = kernel_weight(dpos, inv_dx);
                
                if (weight < 1e-6f) continue;
                
                size_t gid = static_cast<size_t>(k) * grid_nx * grid_ny + 
                            static_cast<size_t>(j) * grid_nx + i;
                
                float3 grid_vel = to_float3(grid[gid].velocity_star);
                
                vel.x += grid_vel.x * weight;
                vel.y += grid_vel.y * weight;
                vel.z += grid_vel.z * weight;
                
                if (apic_factor > 0.0f) {
                    // Compute outer product: grid_vel * dpos^T
                    float3 grad_w = kernel_gradient(dpos, inv_dx);
                    
                    // C += grid_vel * grad(w)^T
                    c[0] += grid_vel.x * grad_w.x * apic_factor;
                    c[1] += grid_vel.x * grad_w.y * apic_factor;
                    c[2] += grid_vel.x * grad_w.z * apic_factor;
                    c[3] += grid_vel.y * grad_w.x * apic_factor;
                    c[4] += grid_vel.y * grad_w.y * apic_factor;
                    c[5] += grid_vel.y * grad_w.z * apic_factor;
                    c[6] += grid_vel.z * grad_w.x * apic_factor;
                    c[7] += grid_vel.z * grad_w.y * apic_factor;
                    c[8] += grid_vel.z * grad_w.z * apic_factor;
                }
            }
        }
    }
    
    // Update position and velocity
    positions[pid] = to_vec3(make_float3(
        pos.x + vel.x * dt,
        pos.y + vel.y * dt,
        pos.z + vel.z * dt
    ));
    
    velocities[pid] = to_vec3(vel);
    
    // Update C matrix
    C[pid] = Mat3(
        c[0], c[1], c[2],
        c[3], c[4], c[5],
        c[6], c[7], c[8]
    );
}

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
) {
    LaunchConfig config = LaunchConfig::for_1d(num_particles, 256);
    g2p_kernel<<<config.grid, config.block, 0, stream>>>(
        positions, velocities, C, grid, num_particles,
        grid_nx, grid_ny, grid_nz, grid_dx,
        dt, apic_factor, pinned, target_positions
    );
    CUDA_CHECK(cudaGetLastError());
}

// Update deformation gradient: F_new = (I + dt * C) * F_old
__global__ void update_F_kernel(
    Mat3* F,
    const Mat3* C,
    size_t num_particles,
    Real dt
) {
    int pid = blockIdx.x * blockDim.x + threadIdx.x;
    if (pid >= num_particles) return;
    
    const Mat3& c_mat = C[pid];
    Mat3& f_mat = F[pid];
    
    // Compute I + dt * C
    Mat3 I(1.0f);
    Mat3 update = I + c_mat * dt;
    
    // F_new = (I + dt * C) * F_old
    f_mat = update * f_mat;
}

void launch_update_F_kernel(
    Mat3* F,
    const Mat3* C,
    size_t num_particles,
    Real dt,
    cudaStream_t stream
) {
    LaunchConfig config = LaunchConfig::for_1d(num_particles, 256);
    update_F_kernel<<<config.grid, config.block, 0, stream>>>(
        F, C, num_particles, dt
    );
    CUDA_CHECK(cudaGetLastError());
}

} // namespace mpm
