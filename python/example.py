#!/usr/bin/env python3
"""
Example usage of mesh2splat Python bindings.

This script demonstrates how to convert GLTF/GLB meshes to 3D Gaussian splats.
"""

import argparse
import sys
from pathlib import Path

# Add the build directory to path for development
# Remove this in production - the module should be installed properly
build_paths = [
    Path(__file__).parent.parent / "build",
    Path(__file__).parent.parent / "build" / "python",
]
for p in build_paths:
    if p.exists():
        sys.path.insert(0, str(p))

import mesh2splat


def main():
    parser = argparse.ArgumentParser(
        description="Convert mesh to Gaussian splats",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python example.py model.gltf output.ply
  python example.py model.glb output.ply --resolution 1024
  python example.py model.gltf output.ply --backend cpu --format pbr
        """,
    )
    parser.add_argument("input", help="Input GLTF/GLB file path")
    parser.add_argument("output", help="Output PLY file path")
    parser.add_argument(
        "--resolution",
        "-r",
        type=int,
        default=512,
        help="UV space rasterization resolution (default: 512)",
    )
    parser.add_argument(
        "--backend",
        "-b",
        choices=["auto", "cpu", "gpu"],
        default="auto",
        help="Backend to use (default: auto)",
    )
    parser.add_argument(
        "--format",
        "-f",
        choices=["standard", "pbr", "compressed"],
        default="standard",
        help="Output PLY format (default: standard)",
    )
    parser.add_argument(
        "--scale",
        "-s",
        type=float,
        default=1.0,
        help="Scale multiplier for gaussians (default: 1.0)",
    )
    parser.add_argument(
        "--no-srgb", action="store_true", help="Disable sRGB conversion"
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Enable verbose output"
    )
    parser.add_argument(
        "--info", action="store_true", help="Print library info and exit"
    )

    args = parser.parse_args()

    # Print info and exit
    if args.info:
        print(f"mesh2splat {mesh2splat.get_version()}")
        print(mesh2splat.get_build_info())
        print(
            f"\nAvailable backends: {[str(b) for b in mesh2splat.Converter.get_available_backends()]}"
        )
        return 0

    # Validate input file
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        return 1

    # Map CLI args to enums
    backend_map = {
        "auto": mesh2splat.Backend.Auto,
        "cpu": mesh2splat.Backend.CPU,
        "gpu": mesh2splat.Backend.GPU,
    }
    format_map = {
        "standard": mesh2splat.PlyFormat.Standard,
        "pbr": mesh2splat.PlyFormat.PBR,
        "compressed": mesh2splat.PlyFormat.Compressed,
    }

    # Create options
    options = mesh2splat.ConversionOptions()
    options.resolution = args.resolution
    options.backend = backend_map[args.backend]
    options.ply_format = format_map[args.format]
    options.scale_multiplier = args.scale
    options.srgb_conversion = not args.no_srgb
    options.verbose = args.verbose

    # Create converter
    print(f"Creating converter...")
    converter = mesh2splat.Converter(options.backend)

    if not converter.is_ready():
        print(
            f"Error: Converter not ready: {converter.get_error_message()}",
            file=sys.stderr,
        )
        return 1

    print(f"Using backend: {converter.get_active_backend()}")
    print(f"Converting: {args.input}")
    print(f"Resolution: {args.resolution}")

    # Convert
    result = converter.convert_file(str(input_path), options)

    if not result.success:
        print(f"Error: Conversion failed: {result.error_message}", file=sys.stderr)
        return 1

    # Print messages
    for msg in result.messages:
        print(f"  {msg}")

    print(
        f"Generated {result.total_gaussians} gaussians from {result.total_triangles} triangles"
    )
    print(f"Conversion time: {result.conversion_time_ms:.2f} ms")

    # Save output
    print(f"Saving to: {args.output}")
    try:
        mesh2splat.PlyIO.save(args.output, result.gaussians, options.ply_format)
    except Exception as e:
        print(f"Error: Failed to save PLY file: {e}", file=sys.stderr)
        return 1

    print("Done!")
    return 0


def demo_numpy():
    """Demonstrate numpy array access."""
    print("\n=== NumPy Array Demo ===\n")

    # This requires a test model
    test_model = (
        Path(__file__).parent.parent
        / "models"
        / "small_lpg_tank_4k"
        / "small_lpg_tank_4k.gltf"
    )

    if not test_model.exists():
        print(f"Test model not found: {test_model}")
        print("Skipping numpy demo")
        return

    converter = mesh2splat.Converter()
    result = converter.convert_file(str(test_model))

    if not result.success:
        print(f"Conversion failed: {result.error_message}")
        return

    # Get numpy arrays
    arrays = result.to_numpy()

    print("Gaussian arrays:")
    print(f"  positions: {arrays['positions'].shape} {arrays['positions'].dtype}")
    print(f"  colors:    {arrays['colors'].shape} {arrays['colors'].dtype}")
    print(f"  opacities: {arrays['opacities'].shape} {arrays['opacities'].dtype}")
    print(f"  scales:    {arrays['scales'].shape} {arrays['scales'].dtype}")
    print(f"  rotations: {arrays['rotations'].shape} {arrays['rotations'].dtype}")
    print(f"  normals:   {arrays['normals'].shape} {arrays['normals'].dtype}")
    print(f"  metallic:  {arrays['metallic'].shape} {arrays['metallic'].dtype}")
    print(f"  roughness: {arrays['roughness'].shape} {arrays['roughness'].dtype}")
    print(f"  ao:        {arrays['ao'].shape} {arrays['ao'].dtype}")

    # Example: compute bounding box
    import numpy as np

    positions = arrays["positions"]
    bbox_min = np.min(positions, axis=0)
    bbox_max = np.max(positions, axis=0)
    print(f"\nBounding box:")
    print(f"  min: {bbox_min}")
    print(f"  max: {bbox_max}")
    print(f"  size: {bbox_max - bbox_min}")


if __name__ == "__main__":
    # Run main CLI
    exit_code = main()

    # Optionally run numpy demo
    if exit_code == 0 and "--demo-numpy" in sys.argv:
        demo_numpy()

    sys.exit(exit_code)
