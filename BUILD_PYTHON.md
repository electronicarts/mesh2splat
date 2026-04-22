# Building Python Wheels for mesh2splat

This guide explains how to build Python wheel packages for mesh2splat.

## Quick Start

```bash
# Build macOS wheels (all Python versions)
make wheels-macos

# Build Linux wheels (all distros via Docker)
make wheels-linux

# Build everything
make wheels-all
```

## Requirements

### macOS

- Python 3.10, 3.11, 3.12 installed
- Xcode Command Line Tools (`xcode-select --install`)
- CMake 3.15+ (`brew install cmake`)

### Linux (via Docker)

- Docker installed and running
- No other dependencies needed (everything runs in containers)

## macOS Builds

### Build All Python Versions

```bash
make wheels-macos
```

Output: `dist/macos/*.whl`

### Build Specific Python Version

```bash
make wheels-macos-3.10
make wheels-macos-3.11
make wheels-macos-3.12
```

### Custom Python Paths

If your Python installations are not in the default locations:

```bash
# Using pyenv
PYTHON310=$(pyenv prefix 3.10.13)/bin/python make wheels-macos-3.10

# Using Homebrew
PYTHON310=/opt/homebrew/bin/python3.10 make wheels-macos-3.10

# Using system Python
PYTHON312=/usr/local/bin/python3.12 make wheels-macos-3.12
```

### Manual Build (without Make)

```bash
# Install build dependencies
python3.10 -m pip install build scikit-build-core pybind11

# Build wheel
python3.10 -m build --wheel -o dist/macos/
```

## Linux Builds (Docker)

See [GUIDE.md](GUIDE.md) for detailed Docker build instructions.

### Build All Linux Distros

```bash
make wheels-linux
```

Output: `dist/linux/<distro>/*.whl`

### Build Specific Distro

```bash
make wheels-linux-manylinux2014    # CentOS 7, glibc 2.17+ (broadest compat)
make wheels-linux-manylinux_2_28   # AlmaLinux 8, glibc 2.28+
make wheels-linux-debian12         # Debian 12 Bookworm
make wheels-linux-ubuntu2204       # Ubuntu 22.04 LTS
make wheels-linux-ubuntu2404       # Ubuntu 24.04 LTS
```

## Output Structure

After building, wheels are organized as:

```
dist/
├── macos/
│   ├── mesh2splat-0.1.0-cp310-cp310-macosx_12_0_arm64.whl
│   ├── mesh2splat-0.1.0-cp311-cp311-macosx_12_0_arm64.whl
│   └── mesh2splat-0.1.0-cp312-cp312-macosx_12_0_arm64.whl
└── linux/
    ├── manylinux2014/
    │   ├── mesh2splat-0.1.0-cp310-cp310-manylinux_2_17_x86_64.manylinux2014_x86_64.whl
    │   ├── mesh2splat-0.1.0-cp311-cp311-manylinux_2_17_x86_64.manylinux2014_x86_64.whl
    │   └── mesh2splat-0.1.0-cp312-cp312-manylinux_2_17_x86_64.manylinux2014_x86_64.whl
    ├── manylinux_2_28/
    │   └── ...
    ├── debian12/
    │   └── mesh2splat-0.1.0-cp311-cp311-linux_x86_64.whl
    ├── ubuntu2204/
    │   └── mesh2splat-0.1.0-cp310-cp310-linux_x86_64.whl
    └── ubuntu2404/
        └── mesh2splat-0.1.0-cp312-cp312-linux_x86_64.whl
```

## Installing Built Wheels

```bash
# Install specific wheel
pip install dist/macos/mesh2splat-0.1.0-cp310-cp310-macosx_12_0_arm64.whl

# Install with upgrade
pip install --upgrade dist/macos/mesh2splat-*.whl

# Verify installation
python -c "import mesh2splat; print(mesh2splat.get_build_info())"
```

## Testing

```bash
# Test installed wheel
make test

# Manual test
python -c "
import mesh2splat
print(f'Version: {mesh2splat.__version__}')
print(f'Build: {mesh2splat.get_build_info()}')
print(f'Backends: {mesh2splat.Converter.get_available_backends()}')
"
```

## Cleaning Build Artifacts

```bash
make clean
```

This removes:
- `build/` - CMake build directory
- `dist/` - Built wheels
- `*.egg-info/` - Package metadata
- `python/mesh2splat/*.so` - Compiled extensions
- `wheelhouse/` - cibuildwheel output

## Troubleshooting

### "Python not found"

Ensure Python is installed and accessible:

```bash
# Check Python versions
which python3.10 python3.11 python3.12

# Or use pyenv
pyenv versions
```

### "CMake version too old"

Update CMake:

```bash
# macOS
brew upgrade cmake

# Linux
pip install --upgrade cmake
```

### "OpenGL headers not found" (Linux)

Install Mesa development packages:

```bash
# Debian/Ubuntu
sudo apt-get install libgl1-mesa-dev libegl1-mesa-dev

# RHEL/CentOS
sudo yum install mesa-libGL-devel mesa-libEGL-devel
```

### Docker build fails

Ensure Docker is running and has enough resources:

```bash
# Check Docker status
docker info

# Clean Docker cache if needed
docker system prune -f
```

## PyPI Upload (Optional)

To upload wheels to PyPI:

```bash
# Install twine
pip install twine

# Upload to TestPyPI first
twine upload --repository testpypi dist/**/*.whl

# Upload to PyPI
twine upload dist/**/*.whl
```
