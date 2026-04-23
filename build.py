#!/usr/bin/env python3
"""
Build script for Blender MPM Solver
Handles compilation of the C++/CUDA core and Python bindings
"""

import os
import sys
import subprocess
import platform
import shutil
from pathlib import Path

def get_project_root():
    """Get the project root directory"""
    return Path(__file__).parent

def get_build_dir():
    """Get the build directory"""
    return get_project_root() / "build"

def check_cuda():
    """Check if CUDA is available"""
    try:
        result = subprocess.run(
            ["nvcc", "--version"],
            capture_output=True,
            text=True,
            check=True
        )
        print("CUDA found:")
        print(result.stdout)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("WARNING: CUDA not found. GPU acceleration will not be available.")
        return False

def check_cmake():
    """Check if CMake is available"""
    try:
        result = subprocess.run(
            ["cmake", "--version"],
            capture_output=True,
            text=True,
            check=True
        )
        version_line = result.stdout.split('\n')[0]
        print(f"CMake found: {version_line}")
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("ERROR: CMake not found. Please install CMake 3.20+")
        return False

def configure_cmake(build_dir, cuda_enabled=True):
    """Configure CMake build"""
    root = get_project_root()
    
    cmake_args = [
        "cmake",
        "-B", str(build_dir),
        "-S", str(root),
        f"-DCMAKE_BUILD_TYPE=Release",
    ]
    
    if not cuda_enabled:
        cmake_args.append("-DCUDA_ENABLED=OFF")
    
    # Platform-specific settings
    if platform.system() == "Windows":
        # Try to find Visual Studio
        cmake_args.extend([
            "-G", "Visual Studio 17 2022",
            "-A", "x64"
        ])
    
    print(f"Configuring with: {' '.join(cmake_args)}")
    
    result = subprocess.run(cmake_args, capture_output=True, text=True)
    if result.returncode != 0:
        print("CMake configuration failed:")
        print(result.stderr)
        return False
    
    print("CMake configuration successful")
    return True

def build_project(build_dir, parallel=True):
    """Build the project"""
    build_args = ["cmake", "--build", str(build_dir), "--config", "Release"]
    
    if parallel:
        build_args.extend(["--parallel", "4"])
    
    print(f"Building with: {' '.join(build_args)}")
    
    result = subprocess.run(build_args, capture_output=True, text=True)
    if result.returncode != 0:
        print("Build failed:")
        print(result.stderr)
        return False
    
    print("Build successful")
    return True

def install_addon(build_dir):
    """Install the addon to the Blender addon directory"""
    root = get_project_root()
    
    # Find the built library
    lib_name = "pympm.pyd" if platform.system() == "Windows" else "pympm.so"
    lib_path = None
    
    for path in build_dir.rglob(lib_name):
        lib_path = path
        break
    
    if not lib_path:
        print(f"ERROR: Could not find built library {lib_name}")
        return False
    
    print(f"Found library at: {lib_path}")
    
    # Copy to addon lib directory
    addon_lib_dir = root / "blender_mpm" / "lib"
    addon_lib_dir.mkdir(parents=True, exist_ok=True)
    
    dest_path = addon_lib_dir / lib_name
    shutil.copy2(lib_path, dest_path)
    print(f"Installed library to: {dest_path}")
    
    # Find and copy CUDA runtime libraries (Windows)
    if platform.system() == "Windows":
        copy_cuda_libs(addon_lib_dir)
    
    return True

def copy_cuda_libs(dest_dir):
    """Copy required CUDA runtime libraries"""
    try:
        # Find CUDA path
        cuda_path = os.environ.get('CUDA_PATH')
        if not cuda_path:
            print("WARNING: CUDA_PATH not set, cannot copy CUDA libraries")
            return
        
        cuda_bin = Path(cuda_path) / "bin"
        
        # Required DLLs
        required_dlls = [
            "cudart64_12.dll",
            "cublas64_12.dll",
            "cublasLt64_12.dll",
        ]
        
        for dll in required_dlls:
            src = cuda_bin / dll
            if src.exists():
                shutil.copy2(src, dest_dir / dll)
                print(f"Copied {dll}")
            else:
                print(f"Warning: Could not find {dll}")
                
    except Exception as e:
        print(f"Warning: Failed to copy CUDA libraries: {e}")

def create_addon_zip():
    """Create a distributable zip file of the addon"""
    root = get_project_root()
    addon_dir = root / "blender_mpm"
    
    zip_path = root / "blender_mpm_addon.zip"
    
    # Create zip
    shutil.make_archive(
        str(root / "blender_mpm_addon"),
        'zip',
        str(root),
        "blender_mpm"
    )
    
    print(f"Created addon zip: {zip_path}")
    return True

def main():
    """Main build function"""
    print("=" * 60)
    print("Blender MPM Solver Build Script")
    print("=" * 60)
    
    # Check prerequisites
    if not check_cmake():
        sys.exit(1)
    
    cuda_available = check_cuda()
    
    # Setup build directory
    build_dir = get_build_dir()
    build_dir.mkdir(exist_ok=True)
    
    # Configure
    if not configure_cmake(build_dir, cuda_available):
        sys.exit(1)
    
    # Build
    if not build_project(build_dir):
        sys.exit(1)
    
    # Install
    if not install_addon(build_dir):
        sys.exit(1)
    
    # Create zip
    create_addon_zip()
    
    print("=" * 60)
    print("Build complete!")
    print(f"Addon location: {get_project_root() / 'blender_mpm'}")
    print(f"Zip file: {get_project_root() / 'blender_mpm_addon.zip'}")
    print("\nTo install in Blender:")
    print("1. Open Blender")
    print("2. Edit > Preferences > Add-ons")
    print("3. Install from disk, select blender_mpm_addon.zip")
    print("4. Enable the 'Physics: MPM Solver' addon")
    print("=" * 60)

if __name__ == "__main__":
    main()
