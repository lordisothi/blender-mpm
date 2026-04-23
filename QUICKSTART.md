# Quick Start Guide

## Your First MPM Simulation

### 1. Create a Scene

```python
import bpy

# Create a cube
bpy.ops.mesh.primitive_cube_add(size=0.5, location=(0, 0, 2))
cube = bpy.context.active_object

# Enable MPM
bpy.context.view_layer.objects.active = cube
cube.mpm_source.enabled = True
cube.mpm_source.material.material_type = 'SNOW'
```

Or use the UI:
1. Add → Mesh → Cube
2. Position it above the grid
3. Physics tab → Enable MPM Source
4. Set Material Type to Snow

### 2. Configure Simulation

In the MPM panel (View3D sidebar):
- Set Grid Spacing: `0.02`
- Set Substeps: `20`
- Set Gravity: `-9.8`

### 3. Initialize and Simulate

Click buttons in order:
1. **Initialize MPM** - Generates particles from mesh
2. **Step** - Run single frame
3. **Bake** - Run full animation and cache

### 4. View Results

Particles will appear as a point cloud object named "MPM_Particles".

## Material Types

| Type | Best For | Key Properties |
|------|----------|----------------|
| **Snow** | Snow, powder, compacting materials | Hardening, compression/stretch limits |
| **Sand** | Sand, gravel, granular materials | Friction angle, cohesion |
| **Elastic** | Rubber, gels, soft bodies | Young's modulus, Poisson ratio |
| **Fluid** | Water, liquids | Bulk modulus, low viscosity |

### Snow Example
```python
cube.mpm_source.material.material_type = 'SNOW'
cube.mpm_source.material.snow_density = 400
cube.mpm_source.material.snow_hardening = 10
```

### Fluid Example
```python
cube.mpm_source.material.material_type = 'FLUID'
cube.mpm_source.material.fluid_density = 1000
cube.mpm_source.material.fluid_bulk_modulus = 2e5
```

## Pinning Particles

1. Select mesh object
2. Enter Edit Mode
3. Create Vertex Group named "pinned"
4. Assign weight 1.0 to vertices you want to pin
5. In MPM Source settings:
   - Enable "Use Pin Group"
   - Select "pinned" group

Pinned particles stay fixed or follow animated targets.

## Tips for Best Results

### Snow
- Higher hardening = stiffer, more cohesive snow
- Critical compression/stretch control when it breaks
- Powder snow: low density, high hardening
- Wet snow: high density, low hardening

### Sand
- Higher friction angle = steeper piles possible
- Cohesion adds stickiness (wet sand)
- Use 30+ substeps for stability

### Fluid
- Higher bulk modulus = less compressible
- Use APIC factor = 1.0 for best vorticity
- Lower grid spacing = finer details

### Elastic
- Young's modulus controls stiffness
- Poisson ratio ~0.5 = incompressible
- Damping reduces oscillations

## Performance Optimization

### If simulation is slow:
1. Increase grid spacing (larger dx = fewer particles)
2. Reduce substeps (trade accuracy for speed)
3. Lower max_particles limit
4. Use coarser source mesh

### If simulation is unstable:
1. Increase substeps (try 30-50)
2. Decrease dt (smaller timestep)
3. Check material parameters (extreme values = explosion)
4. Ensure particles don't start inside colliders

## Common Issues

**"Library not found"**
- Reinstall addon, ensure build completed successfully

**"No particles generated"**
- Check object has MPM Source enabled
- Verify mesh has volume (not just plane)
- Check grid spacing isn't too large

**"Particles disappear"**
- May have fallen out of simulation bounds
- Increase grid padding
- Check boundary condition settings

**"Weird stretching/compression"**
- Adjust critical compression/stretch for snow
- Check for inverted mesh faces
- Ensure proper scale (meters, not millimeters)

## Next Steps

- Explore examples in `examples/` folder
- Read full documentation in README.md
- Try different material presets
- Combine multiple materials in one scene
