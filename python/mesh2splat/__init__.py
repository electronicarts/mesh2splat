"""
mesh2splat - Python bindings for Mesh2Splat

Convert 3D meshes to Gaussian splats for 3D Gaussian Splatting.

Example usage:
    import mesh2splat

    # Simple conversion
    mesh2splat.convert("model.gltf", "output.ply")

    # With options
    options = mesh2splat.ConversionOptions()
    options.resolution = 1024
    options.backend = mesh2splat.Backend.GPU

    converter = mesh2splat.Converter()
    result = converter.convert_file("model.gltf", options)

    if result.success:
        print(f"Generated {result.total_gaussians} gaussians in {result.conversion_time_ms:.2f}ms")
        mesh2splat.PlyIO.save("output.ply", result.gaussians)

        # Access as numpy arrays
        arrays = result.to_numpy()
        positions = arrays["positions"]  # (N, 3) float32
        colors = arrays["colors"]        # (N, 3) float32
"""

try:
    from ._mesh2splat import (
        # Enums
        Backend,
        PlyFormat,
        RasterizationMode,
        DcMode,
        OpacityMode,
        # Core types
        Gaussian,
        ConversionResult,
        ConversionOptions,
        # Main classes
        Converter,
        PlyIO,
        GltfLoader,
        # Module functions
        convert,
        get_version,
        get_build_info,
        gaussians_to_numpy,
    )

    __version__ = get_version()

except ImportError as e:
    # Module not built yet - provide helpful error message
    import sys

    print(
        f"Warning: mesh2splat C++ module not found. Build with CMake first.",
        file=sys.stderr,
    )
    print(f"  mkdir build && cd build", file=sys.stderr)
    print(f"  cmake -DBUILD_PYTHON_BINDINGS=ON ..", file=sys.stderr)
    print(f"  make", file=sys.stderr)
    raise ImportError(
        "mesh2splat C++ module (_mesh2splat) not found. "
        "Please build the project with BUILD_PYTHON_BINDINGS=ON"
    ) from e

__all__ = [
    # Enums
    "Backend",
    "PlyFormat",
    "RasterizationMode",
    "DcMode",
    "OpacityMode",
    # Core types
    "Gaussian",
    "ConversionResult",
    "ConversionOptions",
    # Main classes
    "Converter",
    "PlyIO",
    "GltfLoader",
    # Functions
    "convert",
    "get_version",
    "get_build_info",
    "gaussians_to_numpy",
]
