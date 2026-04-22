# Docker Build Guide for mesh2splat

This guide explains how to use Docker to build mesh2splat Python wheels for various Linux distributions.

## Prerequisites

- Docker installed and running
- Sufficient disk space (~5GB for all images)
- Internet connection (to pull base images)

## Quick Start

```bash
# Build all Linux distros
./docker/build.sh all

# Or use Make
make wheels-linux
```

## Available Distributions

| Distro | Base Image | glibc | Python | Best For |
|--------|-----------|-------|--------|----------|
| `manylinux2014` | CentOS 7 | 2.17+ | 3.10, 3.11, 3.12 | PyPI, broadest compatibility |
| `manylinux_2_28` | AlmaLinux 8 | 2.28+ | 3.10, 3.11, 3.12 | PyPI, newer systems |
| `debian12` | Debian Bookworm | 2.36 | 3.11 | Debian-based systems |
| `ubuntu2204` | Ubuntu 22.04 LTS | 2.35 | 3.10 | Ubuntu LTS |
| `ubuntu2404` | Ubuntu 24.04 LTS | 2.39 | 3.12 | Latest Ubuntu LTS |

## Using the Build Script

### Build All Distros

```bash
./docker/build.sh all
```

### Build Specific Distro

```bash
./docker/build.sh manylinux2014
./docker/build.sh manylinux_2_28
./docker/build.sh debian12
./docker/build.sh ubuntu2204
./docker/build.sh ubuntu2404
```

### Force Rebuild (No Cache)

```bash
./docker/build.sh all --no-cache
./docker/build.sh manylinux2014 --no-cache
```

## Using Make

```bash
# All Linux distros
make wheels-linux

# Specific distros
make wheels-linux-manylinux2014
make wheels-linux-manylinux_2_28
make wheels-linux-debian12
make wheels-linux-ubuntu2204
make wheels-linux-ubuntu2404
```

## Manual Docker Commands

### Build a Specific Image

```bash
# Build the Docker image
docker build -t mesh2splat-manylinux2014 -f docker/Dockerfile.manylinux2014 .

# Extract wheels
mkdir -p dist/linux/manylinux2014
docker run --rm -v $(pwd)/dist/linux/manylinux2014:/output mesh2splat-manylinux2014
```

### Interactive Debugging

```bash
# Start container interactively
docker run -it --rm mesh2splat-manylinux2014 /bin/bash

# Inside container:
ls /build/dist/          # List built wheels
python3.10 -c "import mesh2splat; print(mesh2splat.get_build_info())"
```

### Build with Custom Options

```bash
# Build without GPU support
docker build \
  --build-arg MESH2SPLAT_ENABLE_GPU=OFF \
  -t mesh2splat-cpu-only \
  -f docker/Dockerfile.ubuntu2404 .
```

## Output Structure

Wheels are saved to `dist/linux/<distro>/`:

```
dist/linux/
├── manylinux2014/
│   ├── mesh2splat-0.1.0-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl
│   ├── mesh2splat-0.1.0-cp311-cp311-manylinux_2_17_x86_64.manylinux2014_x86_64.whl
│   └── mesh2splat-0.1.0-cp312-cp312-manylinux_2_17_x86_64.manylinux2014_x86_64.whl
├── manylinux_2_28/
│   ├── mesh2splat-0.1.0-cp310-cp310-manylinux_2_28_x86_64.whl
│   ├── mesh2splat-0.1.0-cp311-cp311-manylinux_2_28_x86_64.whl
│   └── mesh2splat-0.1.0-cp312-cp312-manylinux_2_28_x86_64.whl
├── debian12/
│   └── mesh2splat-0.1.0-cp311-cp311-linux_x86_64.whl
├── ubuntu2204/
│   └── mesh2splat-0.1.0-cp310-cp310-linux_x86_64.whl
└── ubuntu2404/
    └── mesh2splat-0.1.0-cp312-cp312-linux_x86_64.whl
```

## Choosing the Right Distribution

### For PyPI Upload

Use **manylinux2014** for maximum compatibility:
- Works on any Linux with glibc 2.17+ (CentOS 7+, Ubuntu 14.04+, Debian 8+)
- Wheels are automatically audited and repaired with `auditwheel`

Use **manylinux_2_28** for newer systems:
- Works on glibc 2.28+ (CentOS 8+, Ubuntu 18.04+, Debian 10+)
- Better C++17 support, newer compilers

### For Specific Distributions

If you're deploying to a specific environment:
- **debian12**: Debian 12 servers
- **ubuntu2204**: Ubuntu 22.04 LTS servers (common in cloud)
- **ubuntu2404**: Ubuntu 24.04 LTS servers (latest)

## Dockerfile Details

### manylinux2014 / manylinux_2_28

These use the official PyPA manylinux images which include:
- Multiple Python versions (3.10, 3.11, 3.12)
- `auditwheel` for wheel repair
- All necessary build tools

The build process:
1. Install OpenGL/EGL development packages
2. Build wheel for each Python version
3. Repair wheels with `auditwheel` for manylinux compliance
4. Test each wheel

### Debian / Ubuntu

These use standard distribution images:
1. Install build dependencies (CMake, Python, Mesa)
2. Create virtual environment
3. Build single wheel
4. Test the wheel

## GPU Support

All Dockerfiles install Mesa OpenGL/EGL development packages for GPU backend support:

```dockerfile
# manylinux (yum/dnf)
RUN yum install -y mesa-libGL-devel mesa-libEGL-devel

# Debian/Ubuntu (apt)
RUN apt-get install -y libgl1-mesa-dev libegl1-mesa-dev
```

Note: GPU backend requires actual GPU hardware at runtime. The Docker builds include GPU support in the compiled wheels, but testing inside containers uses CPU fallback.

## Troubleshooting

### Docker Not Running

```bash
# Check Docker status
docker info

# Start Docker (macOS)
open -a Docker

# Start Docker (Linux)
sudo systemctl start docker
```

### Permission Denied

```bash
# Add user to docker group (Linux)
sudo usermod -aG docker $USER
# Then logout and login again
```

### Out of Disk Space

```bash
# Clean up Docker
docker system prune -af
docker volume prune -f
```

### Build Fails with Network Error

```bash
# Retry with fresh pull
docker build --no-cache --pull -t mesh2splat-manylinux2014 -f docker/Dockerfile.manylinux2014 .
```

### Wheel Not Found After Build

Check if the build succeeded:

```bash
# List images
docker images | grep mesh2splat

# Run container and check
docker run --rm mesh2splat-manylinux2014 ls -la /build/dist/
```

## Cross-Platform Builds

For ARM64 Linux builds (e.g., for AWS Graviton):

```bash
# Build for linux/arm64
docker buildx build --platform linux/arm64 \
  -t mesh2splat-manylinux2014-arm64 \
  -f docker/Dockerfile.manylinux2014 .
```

Note: This requires Docker buildx and QEMU emulation on x86 hosts.
