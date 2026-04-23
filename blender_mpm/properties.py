import bpy
from bpy.props import (
    FloatProperty, IntProperty, BoolProperty, EnumProperty,
    PointerProperty, CollectionProperty, FloatVectorProperty
)
from bpy.types import PropertyGroup

class MPMMaterialProperties(PropertyGroup):
    """Material properties for MPM particles"""
    
    material_type: EnumProperty(
        name="Material Type",
        items=[
            ('SNOW', "Snow", "Compressible snow with hardening"),
            ('SAND', "Sand", "Granular material with friction"),
            ('ELASTIC', "Elastic", "Rubber-like elastic solid"),
            ('FLUID', "Fluid", "Weakly compressible fluid"),
        ],
        default='SNOW'
    )
    
    # Snow parameters
    snow_density: FloatProperty(name="Density", default=400.0, min=1.0, max=2000.0, unit='DENSITY')
    snow_youngs_modulus: FloatProperty(name="Young's Modulus", default=1.4e5, min=1e3, max=1e7)
    snow_poisson: FloatProperty(name="Poisson Ratio", default=0.2, min=0.0, max=0.49)
    snow_hardening: FloatProperty(name="Hardening", default=10.0, min=0.0, max=50.0)
    snow_critical_compression: FloatProperty(name="Critical Compression", default=2.5e-2, min=1e-3, max=0.5)
    snow_critical_stretch: FloatProperty(name="Critical Stretch", default=7.5e-3, min=1e-3, max=0.5)
    
    # Sand parameters
    sand_density: FloatProperty(name="Density", default=1600.0, min=1.0, max=3000.0, unit='DENSITY')
    sand_youngs_modulus: FloatProperty(name="Young's Modulus", default=3.5e5, min=1e3, max=1e7)
    sand_poisson: FloatProperty(name="Poisson Ratio", default=0.3, min=0.0, max=0.49)
    sand_friction_angle: FloatProperty(name="Friction Angle", default=35.0, min=0.0, max=90.0)
    sand_cohesion: FloatProperty(name="Cohesion", default=0.0, min=0.0, max=10000.0)
    
    # Elastic parameters
    elastic_density: FloatProperty(name="Density", default=1000.0, min=1.0, max=3000.0, unit='DENSITY')
    elastic_youngs_modulus: FloatProperty(name="Young's Modulus", default=1.0e4, min=1e2, max=1e8)
    elastic_poisson: FloatProperty(name="Poisson Ratio", default=0.3, min=0.0, max=0.49)
    elastic_damping: FloatProperty(name="Damping", default=0.0, min=0.0, max=1.0)
    
    # Fluid parameters
    fluid_density: FloatProperty(name="Density", default=1000.0, min=1.0, max=3000.0, unit='DENSITY')
    fluid_bulk_modulus: FloatProperty(name="Bulk Modulus", default=2.0e5, min=1e3, max=1e8)
    fluid_viscosity: FloatProperty(name="Viscosity", default=0.01, min=0.0, max=10.0)

class MPMParticleSourceProperties(PropertyGroup):
    """Properties for objects that emit particles"""
    
    enabled: BoolProperty(name="Enable MPM", default=False)
    
    # Particle generation
    particles_per_cell: IntProperty(name="Particles Per Cell", default=8, min=1, max=64)
    jitter_amount: FloatProperty(name="Jitter", default=0.0, min=0.0, max=1.0)
    
    # Initial velocity
    initial_velocity: FloatVectorProperty(name="Initial Velocity", default=(0.0, 0.0, 0.0), size=3)
    
    # Material reference
    material: PointerProperty(type=MPMMaterialProperties)
    
    # Pinning
    use_pin_group: BoolProperty(name="Use Pin Group", default=False)
    pin_group_name: StringProperty(name="Pin Vertex Group", default="")
    
    # Animated target
    use_target_object: BoolProperty(name="Use Target Object", default=False)
    target_object: PointerProperty(type=bpy.types.Object)

class MPMWorldProperties(PropertyGroup):
    """Global MPM simulation settings"""
    
    # Simulation parameters
    dt: FloatProperty(name="Timestep", default=1.0/60.0, min=1e-4, max=1.0, unit='TIME')
    substeps: IntProperty(name="Substeps", default=20, min=1, max=100)
    gravity: FloatProperty(name="Gravity", default=-9.8, min=-50.0, max=50.0)
    
    # Grid settings
    grid_spacing: FloatProperty(name="Grid Spacing", default=0.02, min=0.001, max=1.0, unit='LENGTH')
    grid_padding: FloatProperty(name="Grid Padding", default=4.0, min=0.0, max=20.0)
    
    # Solver settings
    apic_factor: FloatProperty(name="APIC Factor", default=1.0, min=0.0, max=1.0,
                                description="0=PIC, 1=APIC")
    max_particles: IntProperty(name="Max Particles", default=1000000, min=1000, max=10000000)
    
    # Boundary conditions
    boundary_condition: EnumProperty(
        name="Boundary Condition",
        items=[
            ('STICKY', "Sticky", "Zero velocity at boundaries"),
            ('SEPARATE', "Separate", "Prevent inward motion"),
            ('SLIP', "Slip", "Free boundary"),
        ],
        default='SEPARATE'
    )
    boundary_friction: FloatProperty(name="Boundary Friction", default=0.0, min=0.0, max=1.0)
    
    # Simulation state
    frame: IntProperty(name="Current Frame", default=0)
    is_running: BoolProperty(name="Is Running", default=False)
    cache_directory: StringProperty(name="Cache Directory", default="//cache/mpm", subtype='DIR_PATH')
    
    # Playback
    use_cache: BoolProperty(name="Use Cache", default=True)
    show_particles: BoolProperty(name="Show Particles", default=True)
    particle_display_size: FloatProperty(name="Display Size", default=0.01, min=0.001, max=1.0)

class MPMCacheProperties(PropertyGroup):
    """Properties for cached simulation data"""
    
    is_cached: BoolProperty(name="Is Cached", default=False)
    cache_frame_start: IntProperty(name="Cache Start", default=1)
    cache_frame_end: IntProperty(name="Cache End", default=250)
    cache_filepath: StringProperty(name="Cache File", default="", subtype='FILE_PATH')

# Register property classes
classes = [
    MPMMaterialProperties,
    MPMParticleSourceProperties,
    MPMWorldProperties,
    MPMCacheProperties,
]

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    
    # Add properties to Object type
    bpy.types.Object.mpm_source = PointerProperty(type=MPMParticleSourceProperties)
    bpy.types.Object.mpm_cache = PointerProperty(type=MPMCacheProperties)
    
    # Add material presets
    bpy.types.Scene.mpm_material_presets = CollectionProperty(type=MPMMaterialProperties)

def unregister():
    del bpy.types.Object.mpm_source
    del bpy.types.Object.mpm_cache
    del bpy.types.Scene.mpm_material_presets
    
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
