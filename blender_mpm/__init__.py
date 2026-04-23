bl_info = {
    "name": "MPM Solver",
    "author": "MPM Development Team",
    "version": (1, 0, 0),
    "blender": (5, 0, 0),
    "location": "View3D > Sidebar > MPM",
    "description": "Material Point Method solver for snow, sand, elastic, and fluid simulation",
    "warning": "Requires CUDA-capable GPU",
    "doc_url": "",
    "category": "Physics",
}

import bpy
import sys
import os

# Add the library path
addon_dir = os.path.dirname(__file__)
lib_path = os.path.join(addon_dir, "lib")
if lib_path not in sys.path:
    sys.path.insert(0, lib_path)

try:
    import pympm
    PMPM_AVAILABLE = True
except ImportError:
    PMPM_AVAILABLE = False
    print("WARNING: pympm library not found. Please build the MPM library first.")

# Import all modules
from . import (
    properties,
    operators,
    panels,
    cache,
    utils,
)

modules = [
    properties,
    operators,
    panels,
    cache,
    utils,
]

def register():
    if not PMPM_AVAILABLE:
        print("MPM Solver: Library not available, registering UI only")
    
    for module in modules:
        module.register()
    
    # Register properties on scene
    bpy.types.Scene.mpm_world = bpy.props.PointerProperty(type=properties.MPMWorldProperties)

def unregister():
    for module in reversed(modules):
        module.unregister()
    
    del bpy.types.Scene.mpm_world

if __name__ == "__main__":
    register()
