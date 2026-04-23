"""
Example: Fluid Dam Break
Classic dam break scenario for fluid simulation
"""

import bpy
import numpy as np

def create_dam_break_scene():
    """Create a dam break fluid simulation"""
    
    # Clear scene
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    
    # Create fluid block (the "dam")
    bpy.ops.mesh.primitive_cube_add(size=1, location=(-1, 0, 0.5))
    fluid_block = bpy.context.active_object
    fluid_block.name = "FluidBlock"
    fluid_block.scale = (0.5, 2, 1)
    
    # Enable MPM for fluid
    fluid_block.mpm_source.enabled = True
    fluid_block.mpm_source.material.material_type = 'FLUID'
    fluid_block.mpm_source.material.fluid_density = 1000
    fluid_block.mpm_source.material.fluid_bulk_modulus = 2.0e5
    fluid_block.mpm_source.particles_per_cell = 8
    
    # Create container/tank
    bpy.ops.mesh.primitive_cube_add(size=4, location=(2, 0, 1))
    tank = bpy.context.active_object
    tank.name = "Tank"
    tank.scale = (1, 0.5, 0.5)
    
    # Create obstacle in the path
    bpy.ops.mesh.primitive_cylinder_add(radius=0.3, depth=1, location=(1, 0, 0.3))
    obstacle = bpy.context.active_object
    obstacle.name = "Obstacle"
    
    # World settings for fluid
    world = bpy.context.scene.mpm_world
    world.dt = 1.0 / 60.0
    world.substeps = 20
    world.gravity = -9.8
    world.grid_spacing = 0.02
    world.boundary_condition = 'SEPARATE'
    world.apic_factor = 1.0  # Full APIC for better vorticity preservation
    world.max_particles = 2000000
    
    print("Dam break scene created!")
    print("The fluid block will collapse when simulation starts")
    
    return fluid_block, tank, obstacle

def setup_fluid_rendering():
    """Setup for fluid rendering with transparency"""
    # Create fluid material
    fluid_mat = bpy.data.materials.new(name="WaterMaterial")
    fluid_mat.use_nodes = True
    
    # Set up glass-like shader for water
    bsdf = fluid_mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs['Base Color'].default_value = (0.8, 0.9, 1.0, 1.0)
    bsdf.inputs['Roughness'].default_value = 0.05
    bsdf.inputs['IOR'].default_value = 1.33
    bsdf.inputs['Transmission'].default_value = 0.9
    bsdf.inputs['Alpha'].default_value = 0.8
    
    fluid_mat.blend_method = 'BLEND'
    
    return fluid_mat

if __name__ == "__main__":
    create_dam_break_scene()
    setup_fluid_rendering()
