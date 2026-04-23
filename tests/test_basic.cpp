#include <iostream>
#include <cassert>
#include "mpm/types.h"
#include "mpm/solver.h"
#include "mpm/material.h"

using namespace mpm;

void test_types() {
    std::cout << "Testing types..." << std::endl;
    
    ParticleData data;
    data.resize(10);
    assert(data.count() == 10);
    
    for (size_t i = 0; i < 10; ++i) {
        data.position[i] = Vec3(1.0f, 2.0f, 3.0f);
        data.velocity[i] = Vec3(0.0f);
        data.F[i] = Mat3(1.0f);  // Identity
        data.mass[i] = 1.0f;
        data.volume[i] = 1.0f;
        data.material_type[i] = MaterialType::SNOW;
    }
    
    std::cout << "  Types test passed" << std::endl;
}

void test_grid() {
    std::cout << "Testing grid..." << std::endl;
    
    Vec3 origin(0, 0, 0);
    Real dx = 0.1f;
    Grid grid(origin, dx, 10, 10, 10);
    
    assert(grid.nx == 10);
    assert(grid.ny == 10);
    assert(grid.nz == 10);
    assert(grid.nodes.size() == 1000);
    
    Vec3 node_pos = grid.node_position(5, 5, 5);
    assert(node_pos.x == 5.5f * dx);
    
    std::cout << "  Grid test passed" << std::endl;
}

void test_solver_creation() {
    std::cout << "Testing solver creation..." << std::endl;
    
    MPMSolver solver;
    
    SimParams params;
    params.dt = 1.0f / 60.0f;
    params.substeps = 20;
    solver.set_params(params);
    
    assert(solver.params().dt == 1.0f / 60.0f);
    assert(solver.params().substeps == 20);
    
    std::cout << "  Solver creation test passed" << std::endl;
}

void test_materials() {
    std::cout << "Testing materials..." << std::endl;
    
    SnowMaterial snow;
    SandMaterial sand;
    ElasticMaterial elastic;
    FluidMaterial fluid;
    
    assert(snow.get_default_density() == 400.0f);
    assert(sand.get_default_density() == 1600.0f);
    assert(elastic.get_default_density() == 1000.0f);
    assert(fluid.get_default_density() == 1000.0f);
    
    // Test stress computation (simplified)
    Mat3 F(1.0f);  // Identity
    Mat3 F_elastic(1.0f);
    Real mu = 1e5f;
    Real lambda = 1e5f;
    Mat3 stress;
    
    elastic.compute_stress(F, F_elastic, mu, lambda, stress);
    
    // For identity F, stress should be near zero
    assert(stress[0][0] < 1.0f);
    
    std::cout << "  Materials test passed" << std::endl;
}

int main() {
    std::cout << "Running MPM Basic Tests" << std::endl;
    std::cout << "=======================" << std::endl;
    
    try {
        test_types();
        test_grid();
        test_solver_creation();
        test_materials();
        
        std::cout << "=======================" << std::endl;
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
}
