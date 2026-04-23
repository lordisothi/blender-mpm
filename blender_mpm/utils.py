import bpy
import numpy as np
from mathutils import Vector, Matrix

def get_evaluated_mesh(obj):
    """Get evaluated mesh with modifiers applied"""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    eval_obj = obj.evaluated_get(depsgraph)
    return eval_obj.data

def point_in_mesh(point, mesh, matrix):
    """Check if point is inside mesh using ray casting"""
    # Transform point to mesh local space
    local_point = matrix.inverted() @ Vector(point)
    
    # Simple bounding box test first
    bbox_min = Vector((float('inf'),) * 3)
    bbox_max = Vector((float('-inf'),) * 3)
    
    for vert in mesh.vertices:
        bbox_min = Vector((min(bbox_min[i], vert.co[i]) for i in range(3)))
        bbox_max = Vector((max(bbox_max[i], vert.co[i]) for i in range(3)))
    
    if not all(bbox_min[i] <= local_point[i] <= bbox_max[i] for i in range(3)):
        return False
    
    # Ray casting test (odd number of intersections = inside)
    # Cast ray in +X direction
    ray_origin = local_point
    ray_dir = Vector((1, 0, 0))
    
    hit_count = 0
    for poly in mesh.polygons:
        # Quick bbox check for triangle
        # ... (would need full ray-triangle intersection)
        pass
    
    # Simplified: assume inside if in bbox for now
    return True

def compute_particle_volume(mesh, matrix, num_particles):
    """Compute volume per particle based on mesh volume"""
    # Compute mesh volume using tetrahedralization
    # Simplified: use bounding box approximation
    bbox_min = Vector((float('inf'),) * 3)
    bbox_max = Vector((float('-inf'),) * 3)
    
    for vert in mesh.vertices:
        world_co = matrix @ vert.co
        bbox_min = Vector((min(bbox_min[i], world_co[i]) for i in range(3)))
        bbox_max = Vector((max(bbox_max[i], world_co[i]) for i in range(3)))
    
    bbox_size = bbox_max - bbox_min
    volume = bbox_size.x * bbox_size.y * bbox_size.z
    
    return volume / num_particles if num_particles > 0 else 0

def get_pin_weights(obj, group_name):
    """Get pin weights from vertex group"""
    vgroup = obj.vertex_groups.get(group_name)
    if not vgroup:
        return None
    
    mesh = obj.data
    weights = np.zeros(len(mesh.vertices))
    
    for i, v in enumerate(mesh.vertices):
        try:
            weights[i] = vgroup.weight(i)
        except:
            pass
    
    return weights

def interpolate_vertex_to_point(mesh, point_world, matrix, values):
    """Interpolate per-vertex values to a point in space"""
    # Transform to local space
    local_point = matrix.inverted() @ Vector(point_world)
    
    # Find closest vertex
    min_dist = float('inf')
    closest_idx = -1
    
    for i, vert in enumerate(mesh.vertices):
        dist = (vert.co - local_point).length_squared
        if dist < min_dist:
            min_dist = dist
            closest_idx = i
    
    if closest_idx >= 0 and closest_idx < len(values):
        return values[closest_idx]
    
    return 0

def create_particle_object(name, positions, size=0.01):
    """Create a Blender object to display particles"""
    # Create mesh
    mesh = bpy.data.meshes.new(name + "_mesh")
    obj = bpy.data.objects.new(name, mesh)
    
    # Create vertices
    mesh.vertices.add(len(positions))
    for i, pos in enumerate(positions):
        mesh.vertices[i].co = pos.tolist()
    
    # Set up instancing for visualization (would use geometry nodes in full implementation)
    mesh.update()
    
    return obj

def get_material_presets():
    """Get predefined material presets"""
    return {
        'wet_snow': {
            'type': 'SNOW',
            'density': 500,
            'E': 1.0e5,
            'nu': 0.25,
            'hardening': 5.0,
        },
        'powder_snow': {
            'type': 'SNOW',
            'density': 300,
            'E': 2.0e5,
            'nu': 0.2,
            'hardening': 15.0,
        },
        'dry_sand': {
            'type': 'SAND',
            'density': 1600,
            'E': 3.5e5,
            'nu': 0.3,
            'friction': 35.0,
        },
        'wet_sand': {
            'type': 'SAND',
            'density': 1900,
            'E': 3.0e5,
            'nu': 0.35,
            'friction': 25.0,
            'cohesion': 1000,
        },
        'rubber': {
            'type': 'ELASTIC',
            'density': 1100,
            'E': 1.0e6,
            'nu': 0.45,
        },
        'jelly': {
            'type': 'ELASTIC',
            'density': 1000,
            'E': 1.0e3,
            'nu': 0.48,
        },
        'water': {
            'type': 'FLUID',
            'density': 1000,
            'bulk_modulus': 2.0e5,
        },
    }

def apply_material_preset(material_props, preset_name):
    """Apply a material preset to properties"""
    presets = get_material_presets()
    preset = presets.get(preset_name)
    
    if not preset:
        return False
    
    material_props.material_type = preset['type']
    
    if preset['type'] == 'SNOW':
        material_props.snow_density = preset['density']
        material_props.snow_youngs_modulus = preset['E']
        material_props.snow_poisson = preset['nu']
        material_props.snow_hardening = preset['hardening']
    
    elif preset['type'] == 'SAND':
        material_props.sand_density = preset['density']
        material_props.sand_youngs_modulus = preset['E']
        material_props.sand_poisson = preset['nu']
        material_props.sand_friction_angle = preset['friction']
        if 'cohesion' in preset:
            material_props.sand_cohesion = preset['cohesion']
    
    elif preset['type'] == 'ELASTIC':
        material_props.elastic_density = preset['density']
        material_props.elastic_youngs_modulus = preset['E']
        material_props.elastic_poisson = preset['nu']
    
    elif preset['type'] == 'FLUID':
        material_props.fluid_density = preset['density']
        material_props.fluid_bulk_modulus = preset['bulk_modulus']
    
    return True

def estimate_grid_resolution(particles_count, dx):
    """Estimate grid resolution from particle count"""
    # Assume roughly cubic distribution
    grid_cells_per_dim = int(np.ceil((particles_count / 8) ** (1/3)))
    return grid_cells_per_dim * dx

def compute_cfl_timestep(velocities, dx, safety_factor=0.5):
    """Compute CFL-stable timestep"""
    max_vel = np.max(np.linalg.norm(velocities, axis=1)) if len(velocities) > 0 else 0
    if max_vel < 1e-6:
        return 1.0 / 60.0  # Default
    
    return safety_factor * dx / max_vel

def format_particle_count(count):
    """Format particle count for display"""
    if count >= 1000000:
        return f"{count / 1000000:.2f}M"
    elif count >= 1000:
        return f"{count / 1000:.1f}K"
    else:
        return str(count)

def get_gpu_memory_info():
    """Get GPU memory info if available"""
    try:
        import pympm
        # Would need to add this function to the C++ library
        return "GPU: Available"
    except:
        return "GPU: Not available"

# Progress callback for long operations
class ProgressTracker:
    def __init__(self, wm, total):
        self.wm = wm
        self.total = total
        self.current = 0
    
    def update(self, step=1):
        self.current += step
        progress = self.current / self.total
        self.wm.progress_update(progress)
    
    def finish(self):
        self.wm.progress_end()

def register():
    pass

def unregister():
    pass
