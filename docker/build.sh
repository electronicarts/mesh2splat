#!/bin/bash
# Build mesh2splat wheels for multiple Linux distributions
# Usage: ./build.sh [distro] [--no-cache]
#   distro: manylinux2014, manylinux_2_28, debian12, ubuntu2204, ubuntu2404, all (default: all)
#   --no-cache: Force rebuild without Docker cache

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT_DIR="${PROJECT_DIR}/dist/linux"

# Parse arguments
DISTRO="${1:-all}"
NO_CACHE=""
if [[ "$2" == "--no-cache" ]] || [[ "$1" == "--no-cache" ]]; then
    NO_CACHE="--no-cache"
    if [[ "$1" == "--no-cache" ]]; then
        DISTRO="all"
    fi
fi

# Available distributions
DISTROS=(
    "manylinux2014"
    "manylinux_2_28"
    "debian12"
    "ubuntu2204"
    "ubuntu2404"
)

build_distro() {
    local distro=$1
    local dockerfile="Dockerfile.${distro}"
    local image_name="mesh2splat-${distro}"
    
    echo "=========================================="
    echo "Building for ${distro}..."
    echo "=========================================="
    
    if [[ ! -f "${SCRIPT_DIR}/${dockerfile}" ]]; then
        echo "Error: ${dockerfile} not found"
        return 1
    fi
    
    # Create output directory
    mkdir -p "${OUTPUT_DIR}/${distro}"
    
    # Build Docker image
    docker build ${NO_CACHE} \
        -t "${image_name}" \
        -f "${SCRIPT_DIR}/${dockerfile}" \
        "${PROJECT_DIR}"
    
    # Extract wheels from container
    docker run --rm \
        -v "${OUTPUT_DIR}/${distro}:/output" \
        "${image_name}"
    
    echo "Wheels for ${distro} saved to: ${OUTPUT_DIR}/${distro}/"
    ls -la "${OUTPUT_DIR}/${distro}/"
    echo ""
}

# Main
echo "mesh2splat Linux wheel builder"
echo "=============================="
echo "Project directory: ${PROJECT_DIR}"
echo "Output directory: ${OUTPUT_DIR}"
echo ""

mkdir -p "${OUTPUT_DIR}"

if [[ "$DISTRO" == "all" ]]; then
    for d in "${DISTROS[@]}"; do
        build_distro "$d"
    done
    
    echo "=========================================="
    echo "All builds complete!"
    echo "=========================================="
    echo "Wheels saved to: ${OUTPUT_DIR}/"
    find "${OUTPUT_DIR}" -name "*.whl" -type f
else
    if [[ " ${DISTROS[*]} " =~ " ${DISTRO} " ]]; then
        build_distro "$DISTRO"
    else
        echo "Error: Unknown distro '${DISTRO}'"
        echo "Available distros: ${DISTROS[*]}"
        exit 1
    fi
fi
