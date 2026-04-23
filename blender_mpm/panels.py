import bpy
from bpy.types import Panel

class MPM_PT_main_panel(Panel):
    """Main MPM control panel"""
    bl_label = "MPM Solver"
    bl_idname = "MPM_PT_main_panel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'MPM'
    
    def draw(self, context):
        layout = self.layout
        world = context.scene.mpm_world
        
        # Library status
        try:
            import pympm
            layout.label(text="Library: OK", icon='CHECKMARK')
        except ImportError:
            box = layout.box()
            box.alert = True
            box.label(text="Library Not Found", icon='ERROR')
            box.label(text="Build pympm first")
            return
        
        # Simulation controls
        box = layout.box()
        box.label(text="Simulation", icon='PHYSICS')
        box.prop(world, "frame")
        
        row = box.row(align=True)
        row.operator("mpm.initialize", icon='FILE_NEW')
        row.operator("mpm.reset", icon='X')
        
        row = box.row(align=True)
        row.operator("mpm.step", icon='PLAY')
        row.operator("mpm.bake", icon='RENDER_ANIMATION')
        
        row = box.row()
        row.operator("mpm.clear_cache", icon='TRASH')

class MPM_PT_world_settings(Panel):
    """World/Solver settings"""
    bl_label = "World Settings"
    bl_idname = "MPM_PT_world_settings"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'MPM'
    bl_parent_id = 'MPM_PT_main_panel'
    
    def draw(self, context):
        layout = self.layout
        world = context.scene.mpm_world
        
        # Timestep settings
        box = layout.box()
        box.label(text="Timestep", icon='TIME')
        box.prop(world, "dt")
        box.prop(world, "substeps")
        
        # Physics
        box = layout.box()
        box.label(text="Physics", icon='WORLD')
        box.prop(world, "gravity")
        
        # Grid
        box = layout.box()
        box.label(text="Grid", icon='MESH_GRID')
        box.prop(world, "grid_spacing")
        box.prop(world, "grid_padding")
        
        # Solver
        box = layout.box()
        box.label(text="Solver", icon='PREFERENCES')
        box.prop(world, "apic_factor", slider=True)
        box.prop(world, "max_particles")
        
        # Boundary
        box = layout.box()
        box.label(text="Boundary", icon='MOD_BUILD')
        box.prop(world, "boundary_condition")
        box.prop(world, "boundary_friction")
        
        # Cache
        box = layout.box()
        box.label(text="Cache", icon='FILE_CACHE')
        box.prop(world, "cache_directory")
        box.prop(world, "use_cache")
        
        # Display
        box = layout.box()
        box.label(text="Display", icon='HIDE_OFF')
        box.prop(world, "show_particles")
        box.prop(world, "particle_display_size")

class MPM_PT_object_panel(Panel):
    """Object-specific MPM settings"""
    bl_label = "MPM Source"
    bl_idname = "MPM_PT_object_panel"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = 'physics'
    
    @classmethod
    def poll(cls, context):
        return context.object and context.object.type == 'MESH'
    
    def draw(self, context):
        layout = self.layout
        obj = context.object
        
        if not hasattr(obj, 'mpm_source'):
            layout.label(text="MPM properties not available")
            return
        
        source = obj.mpm_source
        
        # Enable toggle
        layout.prop(source, "enabled", text="Enable MPM")
        
        if not source.enabled:
            return
        
        # Particle generation
        box = layout.box()
        box.label(text="Particles", icon='PARTICLE_DATA')
        box.prop(source, "particles_per_cell")
        box.prop(source, "jitter_amount", slider=True)
        box.prop(source, "initial_velocity")
        
        # Material
        box = layout.box()
        box.label(text="Material", icon='MATERIAL')
        mat = source.material
        
        box.prop(mat, "material_type")
        
        if mat.material_type == 'SNOW':
            box.prop(mat, "snow_density")
            box.prop(mat, "snow_youngs_modulus")
            box.prop(mat, "snow_poisson")
            box.prop(mat, "snow_hardening")
            box.prop(mat, "snow_critical_compression")
            box.prop(mat, "snow_critical_stretch")
        
        elif mat.material_type == 'SAND':
            box.prop(mat, "sand_density")
            box.prop(mat, "sand_youngs_modulus")
            box.prop(mat, "sand_poisson")
            box.prop(mat, "sand_friction_angle")
            box.prop(mat, "sand_cohesion")
        
        elif mat.material_type == 'ELASTIC':
            box.prop(mat, "elastic_density")
            box.prop(mat, "elastic_youngs_modulus")
            box.prop(mat, "elastic_poisson")
            box.prop(mat, "elastic_damping")
        
        elif mat.material_type == 'FLUID':
            box.prop(mat, "fluid_density")
            box.prop(mat, "fluid_bulk_modulus")
            box.prop(mat, "fluid_viscosity")
        
        # Pinning
        box = layout.box()
        box.label(text="Pinning", icon='PINNED')
        box.prop(source, "use_pin_group")
        if source.use_pin_group:
            box.prop_search(source, "pin_group_name", obj, "vertex_groups", text="Group")
        
        # Target
        box = layout.box()
        box.label(text="Target", icon='CONSTRAINT')
        box.prop(source, "use_target_object")
        if source.use_target_object:
            box.prop(source, "target_object")

# Register panels
classes = [
    MPM_PT_main_panel,
    MPM_PT_world_settings,
    MPM_PT_object_panel,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
