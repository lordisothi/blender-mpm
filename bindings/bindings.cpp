#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/functional.h>
#include "mpm/types.h"
#include "mpm/solver.h"
#include "mpm/material.h"

namespace py = pybind11;
using namespace mpm;

// Helper to convert between numpy arrays and glm vectors
Vec3 numpy_to_vec3(py::array_t<float> arr) {
    auto r = arr.unchecked<1>();
    if (r.shape(0) != 3) {
        throw std::runtime_error("Expected array of shape (3,)");
    }
    return Vec3(r(0), r(1), r(2));
}

py::array_t<float> vec3_to_numpy(const Vec3& v) {
    return py::array_t<float>({3}, {sizeof(float)}, &v[0]);
}

// Numpy array wrapper for particle data
class PyParticleData {
public:
    py::array_t<float> positions;
    py::array_t<float> velocities;
    py::array_t<float> deformation_gradients;  // Flattened 3x3 matrices
    py::array_t<float> masses;
    py::array_t<float> volumes;
    py::array_t<int> material_types;
    py::array_t<uint8_t> pinned;
    
    void from_mpm(const ParticleData& data) {
        size_t n = data.count();
        
        // Positions: (n, 3)
        positions = py::array_t<float>({n, size_t(3)});
        auto pos_r = positions.mutable_unchecked<2>();
        for (size_t i = 0; i < n; ++i) {
            pos_r(i, 0) = data.position[i].x;
            pos_r(i, 1) = data.position[i].y;
            pos_r(i, 2) = data.position[i].z;
        }
        
        // Velocities: (n, 3)
        velocities = py::array_t<float>({n, size_t(3)});
        auto vel_r = velocities.mutable_unchecked<2>();
        for (size_t i = 0; i < n; ++i) {
            vel_r(i, 0) = data.velocity[i].x;
            vel_r(i, 1) = data.velocity[i].y;
            vel_r(i, 2) = data.velocity[i].z;
        }
        
        // F matrices: (n, 3, 3) or flattened (n, 9)
        deformation_gradients = py::array_t<float>({n, size_t(9)});
        auto f_r = deformation_gradients.mutable_unchecked<2>();
        for (size_t i = 0; i < n; ++i) {
            const Mat3& m = data.F[i];
            // Column-major order for glm
            f_r(i, 0) = m[0][0]; f_r(i, 1) = m[1][0]; f_r(i, 2) = m[2][0];
            f_r(i, 3) = m[0][1]; f_r(i, 4) = m[1][1]; f_r(i, 5) = m[2][1];
            f_r(i, 6) = m[0][2]; f_r(i, 7) = m[1][2]; f_r(i, 8) = m[2][2];
        }
        
        // Masses: (n,)
        masses = py::array_t<float>(n);
        auto mass_r = masses.mutable_unchecked<1>();
        for (size_t i = 0; i < n; ++i) {
            mass_r(i) = data.mass[i];
        }
        
        // Volumes: (n,)
        volumes = py::array_t<float>(n);
        auto vol_r = volumes.mutable_unchecked<1>();
        for (size_t i = 0; i < n; ++i) {
            vol_r(i) = data.volume[i];
        }
        
        // Material types: (n,)
        material_types = py::array_t<int>(n);
        auto type_r = material_types.mutable_unchecked<1>();
        for (size_t i = 0; i < n; ++i) {
            type_r(i) = static_cast<int>(data.material_type[i]);
        }
        
        // Pinned: (n,)
        pinned = py::array_t<uint8_t>(n);
        auto pin_r = pinned.mutable_unchecked<1>();
        for (size_t i = 0; i < n; ++i) {
            pin_r(i) = data.pinned[i];
        }
    }
    
    ParticleData to_mpm() const {
        ParticleData data;
        
        auto pos_r = positions.unchecked<2>();
        auto vel_r = velocities.unchecked<2>();
        auto f_r = deformation_gradients.unchecked<2>();
        auto mass_r = masses.unchecked<1>();
        auto vol_r = volumes.unchecked<1>();
        auto type_r = material_types.unchecked<1>();
        auto pin_r = pinned.unchecked<1>();
        
        size_t n = pos_r.shape(0);
        data.resize(n);
        
        for (size_t i = 0; i < n; ++i) {
            data.position[i] = Vec3(pos_r(i, 0), pos_r(i, 1), pos_r(i, 2));
            data.velocity[i] = Vec3(vel_r(i, 0), vel_r(i, 1), vel_r(i, 2));
            
            data.F[i] = Mat3(
                f_r(i, 0), f_r(i, 3), f_r(i, 6),
                f_r(i, 1), f_r(i, 4), f_r(i, 7),
                f_r(i, 2), f_r(i, 5), f_r(i, 8)
            );
            
            data.C[i] = Mat3(0);  // Initialize C to zero
            data.mass[i] = mass_r(i);
            data.volume[i] = vol_r(i);
            data.volume0[i] = vol_r(i);
            data.material_type[i] = static_cast<MaterialType>(type_r(i));
            data.pinned[i] = pin_r(i);
            
            // Set default material properties based on type
            switch (data.material_type[i]) {
                case MaterialType::SNOW:
                    data.mu[i] = 1.4e5f / (2.0f * 1.2f);  // E / (2 * (1+nu))
                    data.lambda[i] = 1.4e5f * 0.2f / ((1.0f + 0.2f) * (1.0f - 2.0f * 0.2f));
                    data.hardening[i] = 10.0f;
                    break;
                case MaterialType::SAND:
                    data.mu[i] = 3.5e5f / (2.0f * 1.3f);
                    data.lambda[i] = 3.5e5f * 0.3f / ((1.0f + 0.3f) * (1.0f - 2.0f * 0.3f));
                    data.friction_angle[i] = 35.0f;
                    break;
                case MaterialType::ELASTIC:
                    data.mu[i] = 1.0e4f / (2.0f * 1.3f);
                    data.lambda[i] = 1.0e4f * 0.3f / ((1.0f + 0.3f) * (1.0f - 2.0f * 0.3f));
                    break;
                case MaterialType::FLUID:
                    data.mu[i] = 0.0f;  // Inviscid
                    data.lambda[i] = 2.0e5f;  // Bulk modulus
                    break;
            }
        }
        
        return data;
    }
};

PYBIND11_MODULE(pympm, m) {
    m.doc() = "Material Point Method (MPM) Solver for Blender";
    
    // Material types enum
    py::enum_<MaterialType>(m, "MaterialType")
        .value("SNOW", MaterialType::SNOW)
        .value("SAND", MaterialType::SAND)
        .value("ELASTIC", MaterialType::ELASTIC)
        .value("FLUID", MaterialType::FLUID);
    
    // Simulation parameters
    py::class_<SimParams>(m, "SimParams")
        .def(py::init<>())
        .def_readwrite("dt", &SimParams::dt)
        .def_readwrite("substeps", &SimParams::substeps)
        .def_readwrite("gravity", &SimParams::gravity)
        .def_readwrite("dx", &SimParams::dx)
        .def_readwrite("max_particles", &SimParams::max_particles)
        .def_readwrite("max_grid_nodes", &SimParams::max_grid_nodes)
        .def_readwrite("padding", &SimParams::padding)
        .def_readwrite("apic_factor", &SimParams::apic_factor)
        .def_readwrite("boundary_friction", &SimParams::boundary_friction)
        .def_readwrite("boundary_condition", &SimParams::boundary_condition);
    
    // Material parameters
    py::class_<MaterialParams>(m, "MaterialParams")
        .def(py::init<>())
        .def_readwrite("snow_density", &MaterialParams::snow_density)
        .def_readwrite("snow_E", &MaterialParams::snow_E)
        .def_readwrite("snow_nu", &MaterialParams::snow_nu)
        .def_readwrite("snow_hardening", &MaterialParams::snow_hardening)
        .def_readwrite("snow_critical_compression", &MaterialParams::snow_critical_compression)
        .def_readwrite("snow_critical_stretch", &MaterialParams::snow_critical_stretch)
        .def_readwrite("sand_density", &MaterialParams::sand_density)
        .def_readwrite("sand_E", &MaterialParams::sand_E)
        .def_readwrite("sand_nu", &MaterialParams::sand_nu)
        .def_readwrite("sand_friction_angle", &MaterialParams::sand_friction_angle)
        .def_readwrite("elastic_density", &MaterialParams::elastic_density)
        .def_readwrite("elastic_E", &MaterialParams::elastic_E)
        .def_readwrite("elastic_nu", &MaterialParams::elastic_nu)
        .def_readwrite("fluid_density", &MaterialParams::fluid_density)
        .def_readwrite("fluid_bulk_modulus", &MaterialParams::fluid_bulk_modulus)
        .def_readwrite("fluid_viscosity", &MaterialParams::fluid_viscosity);
    
    // Python-friendly particle data wrapper
    py::class_<PyParticleData>(m, "ParticleData")
        .def(py::init<>())
        .def_readwrite("positions", &PyParticleData::positions)
        .def_readwrite("velocities", &PyParticleData::velocities)
        .def_readwrite("deformation_gradients", &PyParticleData::deformation_gradients)
        .def_readwrite("masses", &PyParticleData::masses)
        .def_readwrite("volumes", &PyParticleData::volumes)
        .def_readwrite("material_types", &PyParticleData::material_types)
        .def_readwrite("pinned", &PyParticleData::pinned);
    
    // Main solver class
    py::class_<MPMSolver>(m, "MPMSolver")
        .def(py::init<>())
        .def("set_params", &MPMSolver::set_params)
        .def("set_material_params", &MPMSolver::set_material_params)
        .def("params", &MPMSolver::params)
        .def("material_params", &MPMSolver::material_params)
        .def("add_particles", [](MPMSolver& solver, const PyParticleData& py_data) {
            solver.add_particles(py_data.to_mpm());
        })
        .def("clear_particles", &MPMSolver::clear_particles)
        .def("particle_count", &MPMSolver::particle_count)
        .def("get_particles", [](MPMSolver& solver) {
            PyParticleData py_data;
            py_data.from_mpm(solver.get_particle_data());
            return py_data;
        })
        .def("initialize", &MPMSolver::initialize)
        .def("step", &MPMSolver::step)
        .def("advance_frame", &MPMSolver::advance_frame)
        .def("save_state", &MPMSolver::save_state)
        .def("load_state", &MPMSolver::load_state);
    
    // Utility functions
    m.def("version", []() { return "1.0.0"; });
    m.def("has_cuda", []() { return true; });
}
