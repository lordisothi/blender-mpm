"""
Example: Snow Ball Drop
Demonstrates a ball of snow falling and colliding with ground
"""

import bpy
import numpy as np

def create_snow_ball_scene():
    """Create the example scene"""
    
    # Clear existing objects
    bpy.ops.object.select_all(action='SELECT')
    bpy.ops.object.delete(use_global=False)
    
    # Create snow ball source
    bpy.ops.mesh.primitive_uv_sphere_add(radius=0.5, location=(0, 0, 2))
    snow_ball = bpy.context.active_object
    snow_ball.name = "SnowBall"
    
    # Enable MPM
    snow_ball.mpm_source.enabled = True
    snow_ball.mpm_source.material.material_type = 'SNOW'
    snow_ball.mpm_source.material.snow_density = 400
    snow_ball.mpm_source.material.snow_youngs_modulus = 1.4e5
    snow_ball.mpm_source.material.snow_hardening = 10.0
    snow_ball.mpm_source.particles_per_cell = 8
    snow_ball.mpm_source.initial_velocity = (0, 0, -2)  # Initial downward velocity
    
    # Create ground plane
    bpy.ops.mesh.primitive_plane_add(size=4, location=(0, 0, 0))
    ground = bpy.context.active_object
    ground.name = "Ground"
    
    # Add some noise to ground for interesting collision
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.subdivide(number_cuts=4)
    bpy.ops.transform.vertex_random(offset=0.05)
    bpy.ops.object.mode_set(mode='OBJECT')
    
    # Configure world settings
    world = bpy.context.scene.mpm_world
    world.dt = 1.0 / 60.0
    world.substeps = 25
    world.gravity = -9.8
    world.grid_spacing = 0.02
    world.boundary_condition = 'SEPARATE'
    world.max_particles = 500000
    
    print("Snow ball scene created!")
    print("Press 'Initialize MPM' in the MPM panel to start")
    
    return snow_ball, ground

def setup_rendering():
    """Setup basic rendering for the simulation"""
    # Set up camera
    bpy.ops.object.camera_add(location=(3, -3, 2))
    camera = bpy.context.active_object
    camera.rotation_euler = (1.1, 0, 0.785)
    bpy.context.scene.camera = camera
    
    # Add lighting
    bpy.ops.object.light_add(type='SUN', location=(5, 5, 10))
    sun = bpy.context.active_object
    sun.data.energy = 3
    
    # Set render engine
    bpy.context.scene.render.engine = 'CYCLES'
    bpy.context.scene.cycles.samples = 128

def run_simulation(frames=120):
    """Run the simulation for specified frames"""
    import pympm
    
    # Initialize
    bpy.ops.mpm.initialize()
    
    # Get solver
    # Note: In real implementation, would need to access the solver instance
    
    print(f"Running simulation for {frames} frames...")
    
    for frame in range(1, frames + 1):
        bpy.ops.mpm.step()
        print(f"Frame {frame}/{frames} complete")
        
        if frame % 10 == 0:
            # Save intermediate state
            bpy.ops.mpm.save_cache()
    
    print("Simulation complete!")

if __name__ == "__main__":
    create_snow_ball_scene()
    setup_rendering()
    # run_simulation()  # Uncomment to auto-run
