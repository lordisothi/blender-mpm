import bpy
import numpy as np
from bpy.types import Operator
from bpy.props import IntProperty, BoolProperty

# Try to import the MPM library
try:
    import pympm
    PMPM_AVAILABLE = True
except ImportError:
    PMPM_AVAILABLE = False
    pympm = None

# Global solver instance (for interactive use)
_solver = None

def get_solver():
    """Get or create the global MPM solver"""
    global _solver
    if _solver is None and PMPM_AVAILABLE:
        _solver = pympm.MPMSolver()
    return _solver

def reset_solver():
    """Reset the global solver"""
    global _solver
    _solver = None

class MPM_OT_initialize(Operator):
    """Initialize the MPM solver and generate particles from source objects"""
    bl_idname = "mpm.initialize"
    bl_label = "Initialize MPM"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        if not PMPM_AVAILABLE:
            self.report({'ERROR'}, "MPM library not available")
            return {'CANCELLED'}
        
        world = context.scene.mpm_world
        solver = get_solver()
        
        # Configure solver
        params = pympm.SimParams()
        params.dt = world.dt
        params.substeps = world.substeps
        params.gravity = world.gravity
        params.dx = world.grid_spacing
        params.padding = world.grid_padding
        params.apic_factor = world.apic_factor
        params.max_particles = world.max_particles
        params.boundary_friction = world.boundary_friction
        params.boundary_condition = ['STICKY', 'SEPARATE', 'SLIP'].index(world.boundary_condition)
        solver.set_params(params)
        
        # Clear existing particles
        solver.clear_particles()
        
        # Generate particles from source objects
        total_particles = 0
        for obj in context.scene.objects:
            if not hasattr(obj, 'mpm_source') or not obj.mpm_source.enabled:
                continue
            
            if obj.type != 'MESH':
                continue
            
            source = obj.mpm_source
            mat = source.material
            
            # Generate particles from mesh
            particles = self.generate_particles_from_mesh(obj, source, world.grid_spacing)
            
            if particles['count'] > 0:
                # Create particle data
                py_data = pympm.ParticleData()
                py_data.positions = particles['positions']
                py_data.velocities = particles['velocities']
                py_data.deformation_gradients = particles['F']
                py_data.masses = particles['masses']
                py_data.volumes = particles['volumes']
                py_data.material_types = particles['material_types']
                py_data.pinned = particles['pinned']
                
                solver.add_particles(py_data)
                total_particles += particles['count']
        
        # Initialize solver
        solver.initialize()
        
        self.report({'INFO'}, f"Initialized with {total_particles} particles")
        return {'FINISHED'}
    
    def generate_particles_from_mesh(self, obj, source, dx):
        """Generate particles from mesh volume"""
        # Ensure mesh is evaluated
        depsgraph = bpy.context.evaluated_depsgraph_get()
        eval_obj = obj.evaluated_get(depsgraph)
        mesh = eval_obj.data
        
        # Get world matrix
        world_matrix = obj.matrix_world
        
        # Compute bounding box
        bbox_min = np.array([float('inf')] * 3)
        bbox_max = np.array([float('-inf')] * 3)
        
        for vert in mesh.vertices:
            co = world_matrix @ vert.co
            bbox_min = np.minimum(bbox_min, np.array(co))
            bbox_max = np.maximum(bbox_max, np.array(co))
        
        # Generate grid points inside mesh
        particles_per_cell = source.particles_per_cell
        ppc_root = int(np.ceil(particles_per_cell ** (1/3)))
        
        grid_min = bbox_min - dx * 2
        grid_max = bbox_max + dx * 2
        
        nx = int(np.ceil((grid_max[0] - grid_min[0]) / dx))
        ny = int(np.ceil((grid_max[1] - grid_min[1]) / dx))
        nz = int(np.ceil((grid_max[2] - grid_min[2]) / dx))
        
        positions = []
        velocities = []
        material_types = []
        pinned_list = []
        
        # Material type mapping
        mat_type_map = {
            'SNOW': 0, 'SAND': 1, 'ELASTIC': 2, 'FLUID': 3
        }
        mat_type = mat_type_map.get(source.material.material_type, 0)
        
        # Check for pin vertex group
        pin_weights = None
        if source.use_pin_group and source.pin_group_name:
            vgroup = obj.vertex_groups.get(source.pin_group_name)
            if vgroup:
                pin_weights = np.zeros(len(mesh.vertices))
                for i, v in enumerate(mesh.vertices):
                    try:
                        pin_weights[i] = vgroup.weight(i)
                    except:
                        pass
        
        # Generate particles
        for i in range(nx):
            for j in range(ny):
                for k in range(nz):
                    cell_min = grid_min + np.array([i, j, k]) * dx
                    
                    # Create multiple particles per cell
                    for pi in range(ppc_root):
                        for pj in range(ppc_root):
                            for pk in range(ppc_root):
                                # Particle position within cell
                                offset = np.array([
                                    (pi + 0.5) / ppc_root,
                                    (pj + 0.5) / ppc_root,
                                    (pk + 0.5) / ppc_root
                                ])
                                
                                pos = cell_min + offset * dx
                                
                                # Check if inside mesh (simplified - use point-in-mesh test)
                                # For now, use bounding box approximation
                                if (bbox_min[0] <= pos[0] <= bbox_max[0] and
                                    bbox_min[1] <= pos[1] <= bbox_max[1] and
                                    bbox_min[2] <= pos[2] <= bbox_max[2]):
                                    
                                    positions.append(pos)
                                    velocities.append(np.array(source.initial_velocity))
                                    material_types.append(mat_type)
                                    pinned_list.append(0)
        
        positions = np.array(positions, dtype=np.float32)
        velocities = np.array(velocities, dtype=np.float32)
        
        count = len(positions)
        if count == 0:
            return {'count': 0}
        
        # Compute volumes
        volume_per_particle = (dx ** 3) / particles_per_cell
        volumes = np.full(count, volume_per_particle, dtype=np.float32)
        
        # Compute masses from density
        density_map = {
            'SNOW': source.material.snow_density,
            'SAND': source.material.sand_density,
            'ELASTIC': source.material.elastic_density,
            'FLUID': source.material.fluid_density
        }
        density = density_map.get(source.material.material_type, 1000.0)
        masses = volumes * density
        
        # Identity deformation gradient
        F = np.tile(np.eye(3, dtype=np.float32).flatten(), (count, 1))
        
        return {
            'count': count,
            'positions': positions,
            'velocities': velocities,
            'F': F,
            'masses': masses,
            'volumes': volumes,
            'material_types': np.array(material_types, dtype=np.int32),
            'pinned': np.array(pinned_list, dtype=np.uint8)
        }

class MPM_OT_step(Operator):
    """Step the simulation forward"""
    bl_idname = "mpm.step"
    bl_label = "Step Simulation"
    bl_options = {'REGISTER'}
    
    def execute(self, context):
        if not PMPM_AVAILABLE:
            self.report({'ERROR'}, "MPM library not available")
            return {'CANCELLED'}
        
        solver = get_solver()
        if solver is None:
            self.report({'ERROR'}, "Solver not initialized")
            return {'CANCELLED'}
        
        world = context.scene.mpm_world
        
        # Step simulation
        solver.advance_frame()
        world.frame += 1
        
        # Update visualization
        self.update_particle_display(context, solver)
        
        return {'FINISHED'}
    
    def update_particle_display(self, context, solver):
        """Update Blender objects to show particle positions"""
        particles = solver.get_particles()
        positions = particles.positions
        
        # Create or update particle object
        particle_obj_name = "MPM_Particles"
        particle_obj = bpy.data.objects.get(particle_obj_name)
        
        if particle_obj is None:
            # Create new mesh
            mesh = bpy.data.meshes.new(particle_obj_name + "_mesh")
            particle_obj = bpy.data.objects.new(particle_obj_name, mesh)
            context.collection.objects.link(particle_obj)
        
        # Update mesh vertices
        mesh = particle_obj.data
        mesh.clear_geometry()
        
        count = len(positions)
        mesh.vertices.add(count)
        for i, pos in enumerate(positions):
            mesh.vertices[i].co = pos.tolist()
        
        mesh.update()

class MPM_OT_bake(Operator):
    """Bake the simulation to cache"""
    bl_idname = "mpm.bake"
    bl_label = "Bake Simulation"
    bl_options = {'REGISTER'}
    
    frame_start: IntProperty(name="Start Frame", default=1)
    frame_end: IntProperty(name="End Frame", default=250)
    
    def execute(self, context):
        if not PMPM_AVAILABLE:
            self.report({'ERROR'}, "MPM library not available")
            return {'CANCELLED'}
        
        solver = get_solver()
        if solver is None:
            self.report({'ERROR'}, "Solver not initialized")
            return {'CANCELLED'}
        
        world = context.scene.mpm_world
        cache_dir = bpy.path.abspath(world.cache_directory)
        
        # Ensure cache directory exists
        import os
        os.makedirs(cache_dir, exist_ok=True)
        
        # Bake frames
        for frame in range(self.frame_start, self.frame_end + 1):
            # Step simulation
            solver.advance_frame()
            
            # Save cache
            cache_file = os.path.join(cache_dir, f"frame_{frame:04d}.bin")
            solver.save_state(cache_file)
            
            print(f"Baked frame {frame}")
        
        world.frame = self.frame_end
        self.report({'INFO'}, f"Baked frames {self.frame_start}-{self.frame_end}")
        return {'FINISHED'}
    
    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

class MPM_OT_clear_cache(Operator):
    """Clear the simulation cache"""
    bl_idname = "mpm.clear_cache"
    bl_label = "Clear Cache"
    bl_options = {'REGISTER'}
    
    def execute(self, context):
        import os
        import shutil
        
        world = context.scene.mpm_world
        cache_dir = bpy.path.abspath(world.cache_directory)
        
        if os.path.exists(cache_dir):
            shutil.rmtree(cache_dir)
            self.report({'INFO'}, f"Cleared cache at {cache_dir}")
        
        world.frame = 0
        return {'FINISHED'}

class MPM_OT_reset(Operator):
    """Reset the simulation"""
    bl_idname = "mpm.reset"
    bl_label = "Reset Simulation"
    bl_options = {'REGISTER'}
    
    def execute(self, context):
        reset_solver()
        context.scene.mpm_world.frame = 0
        
        # Remove particle object
        particle_obj = bpy.data.objects.get("MPM_Particles")
        if particle_obj:
            bpy.data.objects.remove(particle_obj, do_unlink=True)
        
        self.report({'INFO'}, "Simulation reset")
        return {'FINISHED'}

# Register operators
classes = [
    MPM_OT_initialize,
    MPM_OT_step,
    MPM_OT_bake,
    MPM_OT_clear_cache,
    MPM_OT_reset,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
