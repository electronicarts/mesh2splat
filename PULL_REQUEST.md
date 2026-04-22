# Python Bindings + Major Features + 68+ Bug Fixes

## Summary

This PR adds comprehensive Python bindings for headless mesh-to-Gaussian splatting conversion, multiple major GUI features, Docker-based wheel builds, and fixes 68+ bugs identified through extensive code review.

**Branch:** `main-python` -> `main`  
**Commits:** 17  
**Files Changed:** 90+  
**Lines Added:** ~10,600  

---

## New Features

### 1. Python Bindings (`mesh2splat` package)

A complete Python package for headless mesh-to-Gaussian splatting conversion:

- **GPU-accelerated conversion** using EGL headless OpenGL context (Linux)
- **CPU fallback** for systems without GPU support
- **GLTF/GLB loader** with full PBR material support:
  - Base color textures and factors
  - Metallic-roughness maps
  - Normal maps
  - Emissive textures and factors
- **PLY I/O** for reading/writing Gaussian splat files:
  - Standard PLY format
  - Compressed PBR format
- **Flip Y option** for SuperSplat/web viewer compatibility (180 degree X-axis rotation)
- **NumPy integration** for efficient data interchange
- **Configurable conversion parameters**: resolution, samples per face, scale multiplier

```python
import mesh2splat

# Simple one-liner GPU conversion
gaussians = mesh2splat.convert("model.glb", resolution=2048, samples_per_face=16)
mesh2splat.save_ply("output.ply", gaussians, flip_y=True)

# Or use the Converter class for fine-grained control
converter = mesh2splat.Converter(use_gpu=True)
converter.load_gltf("model.glb")
converter.convert(resolution=4096, dc_mode=mesh2splat.DcMode.LINEAR, opacity_mode=mesh2splat.OpacityMode.LINEAR)
converter.save("output.ply")
```

### 2. Build & Distribution System

- **pip installable**: `pip install .` or `pip install mesh2splat`
- **scikit-build-core** integration with CMake backend
- **Docker-based builds** for multiple Linux distros:
  - manylinux2014 (CentOS 7, glibc 2.17+)
  - manylinux_2_28 (AlmaLinux 8, glibc 2.28+)
  - Debian 12 (Bookworm)
  - Ubuntu 22.04 LTS
  - Ubuntu 24.04 LTS
- **Makefile** with convenient build targets
- **Python 3.10, 3.11, 3.12** support
- **GitHub Actions ready** configuration in `pyproject.toml`

### 3. GUI Application Features

| Feature | Description |
|---------|-------------|
| **GLTF/GLB Import** | Full support for loading GLTF 2.0 models (both .gltf ASCII and .glb binary) with embedded textures |
| **Save All Formats** | Export to standard PLY, PBR PLY, and compressed PBR simultaneously with one click |
| **Flip Y Export** | 180 degree X-axis rotation for SuperSplat/web viewer compatibility (avoids mirror artifacts from single-axis reflection) |
| **Orthogonal Projection** | Toggle between perspective and orthographic camera modes for conversion |
| **F-Key Framing** | Press F to frame/focus camera on loaded model bounding box |
| **Auto Output Path** | Automatically sets output path to `source_dir/ply/filename` |

### 4. Conversion System Improvements

- **Two-Pass Conversion System**: Count-only first pass + write pass for better memory management
- **DcMode Enum**: Configurable diffuse color encoding (LINEAR, SRGB)
- **OpacityMode Enum**: Configurable opacity encoding (LINEAR, LOGISTIC)
- **Debug Counters**: SSBO-based counters for conversion diagnostics
- **Resolution Scaling**: Proper handling of high-resolution conversion targets

---

## Bug Fixes (68+ Issues)

### Critical Issues (7)

| # | Issue | File | Fix |
|---|-------|------|-----|
| 1 | **Dangling pointer in batch processing** | `guiRendererConcreteMediator.cpp` | `BatchItem*` pointer invalidated when vector mutates; refactored to index-based access |
| 2 | **Unaligned memory access** | `GltfLoader.cpp` | `reinterpret_cast` on unaligned GLTF buffer data; replaced with `std::memcpy` |
| 3 | **Integer overflow at 8K resolution** | `GPUConverter.cpp` | `int pixelCount = w * h` overflows; changed to `size_t` |
| 4 | **Face normal corruption (CPU)** | `Rasterizer.cpp` | Edge swap modified vectors before cross product; reordered computation |
| 5 | **Face normal corruption (GPU)** | `ShaderManager.cpp` | Same bug in shader code generation; fixed |
| 6 | **Out-of-bounds index access** | `GltfLoader.cpp` | GLTF face indices not bounds-checked; added validation |
| 7 | **GPU resource leak on exception** | `GPUConverter.cpp` | Exceptions left FBOs/textures leaked; added RAII cleanup |

### High Priority Issues (20)

| # | Issue | File | Fix |
|---|-------|------|-----|
| 8 | Incorrect emissiveFactor lookup | `GltfLoader.cpp` | Searched `material.values` instead of `material.additionalValues` |
| 9-12 | Uninitialized GL handles | Multiple render passes | Added `= 0` initialization for all GLuint members |
| 13 | glUnmapBuffer on unmapped buffer | `glUtils.hpp` | Called on never-mapped SSBO; removed invalid call |
| 14 | Shader leak on compile failure | `glUtils.cpp` | Missing `glDeleteShader` on error path |
| 15 | Deep copy of millions of gaussians | `parsers.hpp/cpp` | `savePlyVector` took vector by value; changed to move semantics |
| 16 | Uninitialized Gaussian3D members | `utils.hpp` | Default constructor left fields undefined |
| 17 | Missing file I/O error checking | `PlyIO.cpp` | PLY writer didn't check file open success |
| 18 | Uninitialized memory from reserve() | `GltfLoader.cpp` | `reserve()` then index access instead of `push_back()` |
| 19 | Memory leak in loadImageAndBpp | `parsers.cpp` | stb_image allocation not freed on error |
| 20 | OctWrap sign logic error | `parsers.cpp` | Incorrect octahedral encoding/decoding |
| 21 | Quaternion write order in PLY | `PlyIO.cpp` | Wrong component order (should be w,x,y,z) |
| 22 | Swapped quaternion in CPUConverter | `CPUConverter.cpp` | Quaternion components in wrong order |
| 23 | convertMeshToSplat ignoring options | `Converter.cpp` | User options (scaleMultiplier, flipY) not passed through |
| 24 | byteStride for interleaved buffers | `GltfLoader.cpp` | Incorrect stride calculation for interleaved vertex data |
| 25 | Buffer handle leak | `GPUConverter.cpp` | fillGaussianBufferSsbo leaked handles |
| 26 | 2GB per-frame allocation | `GaussianShadowPass.cpp` | Large allocation in render loop; moved to constructor |
| 27 | Renderer destructor missing cleanup | `renderer.cpp` | Missing `glDeleteBuffers` for created buffers |

### Medium Priority Issues (14)

| # | Issue | File | Fix |
|---|-------|------|-----|
| 28 | Deprecated `std::experimental::filesystem` | `utils.hpp/cpp` | Updated to C++17 `std::filesystem` |
| 29 | ODR violation risk | `utils.hpp` | `static` functions in headers changed to `inline` |
| 30 | O(N) vector erase for rolling buffer | `ImGuiUI.cpp` | Changed to `std::deque` for O(1) `pop_front()` |
| 31 | Hardcoded Windows path separator | `ImGuiUI.cpp` | Changed to `std::filesystem::path` for cross-platform |
| 32 | `#pragma once` in .cpp file | `ShaderRegistry.cpp` | Removed ineffective pragma |
| 33 | BBox accumulation across meshes | `SceneManager.cpp` | Fixed bounding box calculation for multi-mesh models |
| 34 | emissiveFactor default value | `GltfLoader.cpp` | Changed default from (1,1,1) to (0,0,0) per glTF spec |
| 35 | isValid() scale logic | `Types.cpp` | Used OR instead of AND for scale validation |
| 36 | Degenerate triangle NaN | `Rasterizer.cpp` | Added cross product guard for zero-area triangles |
| 37 | GenerateTangent NaN | `parsers.cpp` | Fixed cross product with zero vector |
| 38 | mergeChildGaussians crash | Multiple files | Fixed crash on empty input |
| 39 | conversionDebugCounters SSBO | `ConversionPass.cpp` | Fixed SSBO creation and binding |
| 40 | div-by-zero in shadow pass | `GaussianShadowPass.cpp` | Added guard for zero divisor |
| 41 | Linux build issues | Multiple files | Case-sensitive includes, link stdc++fs, missing headers |

### Low Priority Issues (12)

- String parameters by value instead of const reference
- Redundant vector reallocations
- Magic numbers without named constants
- Missing `noexcept` specifications
- Inconsistent error handling patterns

### Style Issues (15+)

- Missing `override` keywords on virtual methods across all render passes
- Inconsistent naming and formatting
- Commented-out dead code

### GUI-Specific Fixes

| Issue | Description |
|-------|-------------|
| **Black screen bug** | FBO setup order issue: `glBindFramebuffer(0)` was called BEFORE `glDrawBuffer/glReadBuffer` instead of after |
| **Shader typo** | `u_isLightingEnalbed` -> `u_isLightingEnabled` in gaussianSplattingDeferredPS.glsl |
| **Example.py false error** | `PlyIO.save` returns None; fixed error handling to use try/except |

---

## Files Changed

### New Files (45+)

```
python/                              # Python bindings package
├── CMakeLists.txt                   # CMake build for Python module
├── mesh2splat/__init__.py           # Package interface
├── example.py                       # Usage examples
└── src/mesh2splat/
    ├── Bindings.cpp                 # pybind11 bindings
    ├── Converter.cpp/hpp            # High-level converter API
    ├── core/
    │   ├── GltfLoader.cpp/hpp       # GLTF/GLB loading with PBR materials
    │   ├── PlyIO.cpp/hpp            # PLY read/write (standard + compressed)
    │   ├── TextureSampler.cpp/hpp   # Texture sampling utilities
    │   └── Types.cpp/hpp            # Data structures (Gaussian, Material, etc.)
    ├── cpu/
    │   ├── CPUConverter.cpp/hpp     # CPU-based conversion
    │   └── Rasterizer.cpp/hpp       # Triangle rasterization
    └── gpu/
        ├── GLLoader.cpp/hpp         # EGL/GL function loader for Linux
        ├── GPUConverter.cpp/hpp     # GPU-accelerated conversion
        ├── HeadlessContext.cpp/hpp  # EGL headless OpenGL context
        └── ShaderManager.cpp/hpp    # Shader compilation and management

docker/                              # Multi-distro Docker builds
├── build.sh                         # Main build script
├── Dockerfile.manylinux2014
├── Dockerfile.manylinux_2_28
├── Dockerfile.debian12
├── Dockerfile.ubuntu2204
└── Dockerfile.ubuntu2404

pyproject.toml                       # Python package configuration
Makefile                             # Build automation
BUILD_PYTHON.md                      # Build documentation
.dockerignore                        # Docker build optimization
requirements.txt                     # Python dependencies
uv.lock                              # Reproducible builds with uv
```

### Modified Files (40+)

**Build System:**
- `CMakeLists.txt` - Python bindings integration, Linux build fixes

**GUI Application:**
- `src/renderer/renderer.cpp` - FBO setup order fix, destructor cleanup
- `src/renderer/guiRendererConcreteMediator.cpp/hpp` - Batch processing fix (dangling pointer)
- `src/imGuiUi/ImGuiUI.cpp/hpp` - New features, deque optimization
- `src/utils/utils.hpp/cpp` - Filesystem updates, inline functions
- `src/utils/glUtils.cpp/hpp` - Shader deletion fix, unmapBuffer fix
- `src/utils/SceneManager.cpp` - BBox fix
- `src/parsers/parsers.cpp/hpp` - Move semantics, memory fixes
- `src/parsers/GaussianSplat.cpp/h` - New merge helpers, BoundingBox struct

**Render Passes:**
- `src/renderer/renderPasses/GaussianSplattingPass.hpp` - GL init, override
- `src/renderer/renderPasses/GaussianShadowPass.cpp/hpp` - Memory fix, GL init
- `src/renderer/renderPasses/GaussianRelightingPass.cpp/hpp` - GL init, override
- `src/renderer/renderPasses/ConversionPass.cpp/hpp` - Two-pass system
- `src/renderer/renderPasses/RenderContext.hpp` - GL initialization
- `src/renderer/renderPasses/*.hpp` - override keywords throughout

**Shaders:**
- `src/shaders/conversion/converterFS.glsl` - Two-pass improvements
- `src/shaders/conversion/converterGS.glsl` - Debug counters
- `src/shaders/rendering/gaussianSplattingDeferredPS.glsl` - Typo fix

---

## Build Instructions

### Python Package

```bash
# Development install
pip install -e .

# Build wheel
pip wheel . -w dist/

# Docker builds (all distros)
cd docker && ./build.sh all

# Or use Makefile
make wheels-linux        # All Linux distros
make wheels-macos        # macOS (if on macOS)
```

### C++ GUI

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

---

## Testing

- GUI tested with multiple GLTF/GLB models (duck.glb, small_lpg_tank_4k.gltf)
- Python bindings tested with `python/example.py`
- Built wheels for Python 3.10, 3.11, 3.12 on:
  - manylinux2014 (CP312 manylinux-compliant)
  - manylinux_2_28 (CP312 manylinux-compliant)
  - Debian 12, Ubuntu 22.04, Ubuntu 24.04
- All C++ code compiles cleanly with `-Wall -Wextra`

---

## Breaking Changes

**None** - All changes are additive or fix existing bugs. Existing functionality is preserved.

---

## Documentation

| File | Description |
|------|-------------|
| `BUILD_PYTHON.md` | Complete guide for building Python wheels |
| `CODE_REVIEW.md` | Detailed documentation of all 53+ code review issues |
| `GUIDE.md` | Usage guide for the mesh2splat package |
| `python/example.py` | Working examples of Python API |

---

## Checklist

- [x] Code compiles without warnings
- [x] GUI application tested and working
- [x] Python bindings tested and working
- [x] Docker builds produce valid wheels
- [x] All critical bugs fixed
- [x] Documentation updated
