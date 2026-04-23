#include "mpm/types.h"

namespace mpm {

void ParticleData::resize(size_t n) {
    position.resize(n);
    velocity.resize(n);
    C.resize(n);
    F.resize(n);
    mass.resize(n);
    volume.resize(n);
    volume0.resize(n);
    material_type.resize(n);
    mu.resize(n);
    lambda.resize(n);
    hardening.resize(n);
    pinned.resize(n);
    target_position.resize(n);
    friction_angle.resize(n);
    cohesion.resize(n);
}

void ParticleData::clear() {
    position.clear();
    velocity.clear();
    C.clear();
    F.clear();
    mass.clear();
    volume.clear();
    volume0.clear();
    material_type.clear();
    mu.clear();
    lambda.clear();
    hardening.clear();
    pinned.clear();
    target_position.clear();
    friction_angle.clear();
    cohesion.clear();
}

Grid::Grid(const Vec3& origin, Real dx, int nx, int ny, int nz)
    : origin(origin), dx(dx), nx(nx), ny(ny), nz(nz) {
    nodes.resize(nx * ny * nz);
    clear();
}

void Grid::resize(int nx_, int ny_, int nz_) {
    nx = nx_;
    ny = ny_;
    nz = nz_;
    nodes.resize(nx * ny * nz);
    clear();
}

void Grid::clear() {
    for (auto& node : nodes) {
        node.velocity = Vec3(0);
        node.velocity_star = Vec3(0);
        node.mass = 0;
        node.active = 0;
    }
}

size_t Grid::index(int i, int j, int k) const {
    return static_cast<size_t>(k) * nx * ny + static_cast<size_t>(j) * nx + i;
}

Vec3 Grid::node_position(int i, int j, int k) const {
    return origin + Vec3(i * dx, j * dx, k * dx);
}

void BBox::expand(const Vec3& p) {
    min = glm::min(min, p);
    max = glm::max(max, p);
}

void BBox::expand(const BBox& other) {
    expand(other.min);
    expand(other.max);
}

} // namespace mpm
