# GitHub Actions Setup Guide

## Quick Steps (5 minutes)

### 1. Create GitHub Repository

```bash
# Initialize git repo
git init
git add .
git commit -m "Initial MPM Solver commit"

# Create repo on GitHub (via web or gh CLI)
gh repo create blender-mpm --public --source=. --push
```

Or manually:
1. Go to https://github.com/new
2. Name: `blender-mpm`
3. Don't initialize with README (we have one)
4. Create repository
5. Follow "push an existing repository" instructions

### 2. Push Code

```bash
git remote add origin https://github.com/YOUR_USERNAME/blender-mpm.git
git branch -M main
git push -u origin main
```

### 3. Wait for Build

- Go to **Actions** tab on GitHub
- Build starts automatically (takes ~15 minutes)
- Download `blender_mpm_addon_windows.zip` from Artifacts

### 4. Install in Blender

1. Download the artifact from GitHub
2. Blender → Edit → Preferences → Add-ons → Install
3. Select the zip file
4. Enable "Physics: MPM Solver"

## How It Works

```
You push code ─────────────────┐
                               │
    ┌──────────────────────────▼────────────┐
    │     GitHub Actions (Cloud Servers)    │
    │  ┌─────────────────────────────────┐  │
    │  │  Windows Server 2022              │  │
    │  │  ├─ Visual Studio 2022            │  │
    │  │  ├─ CUDA Toolkit 12.1            │  │
    │  │  ├─ Python 3.11                  │  │
    │  │  └─ CMake                        │  │
    │  │                                 │  │
    │  │  Build Steps:                  │  │
    │  │  1. Checkout code               │  │
    │  │  2. Configure CMake             │  │
    │  │  3. Compile C++/CUDA            │  │
    │  │  4. Create addon zip            │  │
    │  │  5. Upload artifact             │  │
    │  └─────────────────────────────────┘  │
    └─────────────────────────────────────────┘
                               │
                    ┌──────────▼──────────┐
                    │   Download Zip      │
                    └──────────┬──────────┘
                               │
                    ┌──────────▼──────────┐
                    │  Install in Blender │
                    └─────────────────────┘
```

## Automatic Builds

The workflow triggers on:

| Event | What Happens |
|-------|--------------|
| Push to `main` | Build + upload artifact |
| Pull request | Build + run tests |
| Create release | Build + attach to release |

## Manual Trigger

You can also run builds manually:

1. Go to **Actions** tab
2. Click **Build MPM Solver**
3. Click **Run workflow**
4. Select branch
5. Click **Run workflow**

## Troubleshooting

### Build fails

Check the build log in GitHub Actions:
1. Click on failed workflow run
2. Click on the failed job (e.g., `build-windows`)
3. Expand the failing step to see error

### Common issues

| Issue | Solution |
|-------|----------|
| CUDA not found | Check `Jimver/cuda-toolkit` action version |
| CMake fails | Check paths in workflow file |
| Link errors | Check CUDA architecture compatibility |

## Advanced: Self-Hosted Runner

If you have a powerful GPU machine, you can use it as a runner:

1. Install GitHub Actions runner on your machine
2. Add runner to your repository
3. Update workflow to use your runner

This gives you faster builds and access to your specific GPU.

## Cost

**GitHub Actions is FREE for public repositories:**
- 2,000 minutes/month (Linux)
- 200 minutes/month (Windows) - but public repos have unlimited
- 500 MB storage

For this project: **Completely free** if repo is public.

## Next Steps

1. [Create repository](https://github.com/new)
2. Push this code
3. Wait 15 minutes
4. Download and install!
