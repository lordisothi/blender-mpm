# Installation Guide

## Quick Start

### Option 1: Pre-built Binary (Recommended for Users)

1. Download `blender_mpm_addon.zip` from the [Releases](https://github.com/yourusername/blender-mpm/releases) page
2. Open Blender
3. Edit → Preferences → Add-ons → Install...
4. Select the downloaded zip file
5. Enable the "Physics: MPM Solver" addon
6. The MPM panel will appear in the View3D sidebar (N panel)

### Option 2: Build from Source (For Developers)

#### Prerequisites

**Windows:**
- Visual Studio 2022 (with C++ and CUDA support)
- CUDA Toolkit 12.x
- CMake 3.20+
- Python 3.11+ (matching Blender's Python version)

**Linux:**
```bash
sudo apt-get update
sudo apt-get install build-essential cmake git
# Install CUDA toolkit from NVIDIA
```

**macOS:**
- Note: CUDA is not available on macOS. Use CPU-only build or Metal backend (experimental).

#### Build Steps

1. **Clone the repository:**
```bash
git clone https://github.com/yourusername/blender-mpm.git
cd blender-mpm
```

2. **Install Python dependencies:**
```bash
pip install pybind11 numpy
```

3. **Build the project:**
```bash
python build.py
```

Or manually:
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

4. **Install to Blender:**

The build script will automatically copy the library to `blender_mpm/lib/`.

To install in Blender:
```bash
# Create zip for installation
cd ..
python build.py --zip

# Or manually zip the blender_mpm folder
```

Then install the zip in Blender preferences.

#### Troubleshooting Build Issues

**"CMake Error: Could not find CUDA"**
- Ensure CUDA toolkit is installed and `nvcc` is in PATH
- Set `CUDA_TOOLKIT_ROOT_DIR` environment variable

**"pybind11 not found"**
```bash
pip install pybind11
# Or:
python -m pip install pybind11
```

**"Python version mismatch"**
- Blender 5.0 uses Python 3.11
- Ensure your system Python matches
- Or use Blender's bundled Python: `blender --python-exec python3.11`

**Windows-specific: "Cannot find vcvarsall.bat"**
- Run from "Developer Command Prompt for VS 2022"
- Or ensure Visual Studio C++ tools are installed

**Linux-specific: "CUDA not found"**
```bash
# Add to .bashrc or .zshrc
export PATH=/usr/local/cuda/bin:$PATH
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
```

### Option 3: Development Install (Editable)

For development and testing:

```bash
# Build
cd blender-mpm
python build.py

# Create symlink to Blender addons folder (Linux/Mac)
ln -s $(pwd)/blender_mpm ~/Library/Application\ Support/Blender/5.0/scripts/addons/

# Or copy (Windows)
xcopy /E /I blender_mpm "%USERPROFILE%\AppData\Roaming\Blender Foundation\Blender\5.0\scripts\addons\blender_mpm"
```

## Verifying Installation

1. Open Blender
2. Check the System Console (Window → Toggle System Console)
3. Look for: `MPM Solver: Library loaded successfully`
4. Open View3D → N panel → MPM tab
5. Click "Initialize MPM" with a mesh object selected

## Uninstallation

**Via Blender:**
1. Edit → Preferences → Add-ons
2. Find "Physics: MPM Solver"
3. Disable, then Remove

**Manual:**
- Delete the addon folder from Blender's scripts directory
- Remove cache files manually if needed

## System Requirements

### Minimum
- GPU: NVIDIA GTX 1060 (6GB) or equivalent
- RAM: 8GB
- CPU: Any modern x64 processor

### Recommended
- GPU: NVIDIA RTX 3060 or better (for millions of particles)
- RAM: 32GB
- CPU: Multi-core for preprocessing

### For Large Simulations (5M+ particles)
- GPU: NVIDIA RTX 4090 or A6000
- RAM: 64GB+
- VRAM: 24GB+

## Getting Help

- **GitHub Issues**: Report bugs and feature requests
- **Documentation**: See README.md and examples/
- **Discord**: [Join our community](https://discord.gg/yourlink)
