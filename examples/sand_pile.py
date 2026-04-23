"""
Example: Sand Pile
Demonstrates granular material piling up with friction
"""

import bpy
import numpy as np

def create_sand_pile_scene():
    """Create a sand pile simulation scene"""
    
    # Clear scene
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    
    # Create sand source (box that will emit particles)
    bpy.ops.mesh.primitive_cube_add(size=0.4, location=(0, 0, 3))
    sand_source = bpy.context.active_object
    sand_source.name = "SandSource"
    
    # Enable MPM
    sand_source.mpm_source.enabled = True
    sand_source.mpm_source.material.material_type = 'SAND'
    sand_source.mpm_source.material.sand_density = 1600
    sand_source.mpm_source.material.sand_friction_angle = 35.0
    sand_source.mpm_source.particles_per_cell = 8
    sand_source.mpm_source.initial_velocity = (0, 0, 0)
    
    # Create container (box with open top)
    bpy.ops.mesh.primitive_cube_add(size=2, location=(0, 0, 0.5))
    container = bpy.context.active_object
    container.name = "Container"
    container.scale = (1, 1, 0.5)
    
    # Make it a collision object (simplified - real implementation would need SDF)
    
    # Create ground
    bpy.ops.mesh.primitive_plane_add(size=10, location=(0, 0, 0))
    ground = bpy.context.active_object
    ground.name = "Ground"
    
    # World settings
    world = bpy.context.scene.mpm_world
    world.dt = 1.0 / 60.0
    world.substeps = 30  # Higher substeps for stability
    world.gravity = -9.8
    world.grid_spacing = 0.015  # Finer grid for sand
    world.boundary_condition = 'SEPARATE'
    world.boundary_friction = 0.5
    world.max_particles = 1000000
    
    print("Sand pile scene created!")
    print("Note: Sand simulation requires high friction settings")
    
    return sand_source, container, ground

def create_materials():
    """Create materials for visualization"""
    # Sand material
    sand_mat = bpy.data.materials.new(name="SandMaterial")
    sand_mat.use_nodes = True
    
    # Set up basic sand color
    bsdf = sand_mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs['Base Color'].default_value = (0.8, 0.7, 0.5, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.9
    
    return sand_mat

if __name__ == "__main__":
    create_sand_pile_scene()
    create_materials()
