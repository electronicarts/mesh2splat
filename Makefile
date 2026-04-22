# Mesh2Splat Python Wheel Build Makefile
# =======================================
#
# Usage:
#   make dev-env            # Setup development environment (uv + Python 3.10-3.12)
#   make wheels-macos       # Build macOS wheels (Python 3.10, 3.11, 3.12)
#   make wheels-linux       # Build all Linux wheels via Docker
#   make wheels-linux-manylinux2014  # Build specific Linux distro
#   make wheels-all         # Build all platforms
#   make clean              # Clean build artifacts
#   make test               # Test installed wheel

.PHONY: all clean wheels-macos wheels-linux wheels-all test help dev-env \
        wheels-linux-manylinux2014 wheels-linux-manylinux_2_28 \
        wheels-linux-debian12 wheels-linux-ubuntu2204 wheels-linux-ubuntu2404

# Python versions to build for macOS (use uv-managed pythons if available)
PYTHON310 ?= $(shell uv python find 3.10 2>/dev/null || echo python3.10)
PYTHON311 ?= $(shell uv python find 3.11 2>/dev/null || echo python3.11)
PYTHON312 ?= $(shell uv python find 3.12 2>/dev/null || echo python3.12)

# Directories
DIST_DIR := dist
DIST_MACOS := $(DIST_DIR)/macos
DIST_LINUX := $(DIST_DIR)/linux

# Default target
all: help

help:
	@echo "Mesh2Splat Wheel Builder"
	@echo "========================"
	@echo ""
	@echo "Setup:"
	@echo "  make dev-env               Setup dev environment (uv + Python 3.10-3.12)"
	@echo ""
	@echo "macOS targets:"
	@echo "  make wheels-macos          Build wheels for Python 3.10, 3.11, 3.12"
	@echo "  make wheels-macos-3.10     Build wheel for Python 3.10 only"
	@echo "  make wheels-macos-3.11     Build wheel for Python 3.11 only"
	@echo "  make wheels-macos-3.12     Build wheel for Python 3.12 only"
	@echo ""
	@echo "Linux targets (via Docker):"
	@echo "  make wheels-linux          Build all Linux distros"
	@echo "  make wheels-linux-manylinux2014   CentOS 7, glibc 2.17+"
	@echo "  make wheels-linux-manylinux_2_28  AlmaLinux 8, glibc 2.28+"
	@echo "  make wheels-linux-debian12        Debian 12 Bookworm"
	@echo "  make wheels-linux-ubuntu2204      Ubuntu 22.04 LTS"
	@echo "  make wheels-linux-ubuntu2404      Ubuntu 24.04 LTS"
	@echo ""
	@echo "Combined targets:"
	@echo "  make wheels-all            Build all platforms"
	@echo "  make clean                 Clean build artifacts"
	@echo "  make test                  Test wheel import"

# ============================================================================
# macOS Builds
# ============================================================================

$(DIST_MACOS):
	mkdir -p $(DIST_MACOS)

.PHONY: wheels-macos-3.10
wheels-macos-3.10: $(DIST_MACOS)
	@echo "Building wheel for Python 3.10..."
	uv build --python 3.10 --wheel -o $(DIST_MACOS)
	@echo "Done: $$(ls $(DIST_MACOS)/*cp310*.whl 2>/dev/null | tail -1)"

.PHONY: wheels-macos-3.11
wheels-macos-3.11: $(DIST_MACOS)
	@echo "Building wheel for Python 3.11..."
	uv build --python 3.11 --wheel -o $(DIST_MACOS)
	@echo "Done: $$(ls $(DIST_MACOS)/*cp311*.whl 2>/dev/null | tail -1)"

.PHONY: wheels-macos-3.12
wheels-macos-3.12: $(DIST_MACOS)
	@echo "Building wheel for Python 3.12..."
	uv build --python 3.12 --wheel -o $(DIST_MACOS)
	@echo "Done: $$(ls $(DIST_MACOS)/*cp312*.whl 2>/dev/null | tail -1)"

wheels-macos: wheels-macos-3.10 wheels-macos-3.11 wheels-macos-3.12
	@echo ""
	@echo "macOS wheels built:"
	@ls -la $(DIST_MACOS)/*.whl

# ============================================================================
# Linux Builds (Docker)
# ============================================================================

$(DIST_LINUX):
	mkdir -p $(DIST_LINUX)

.PHONY: wheels-linux-manylinux2014
wheels-linux-manylinux2014: $(DIST_LINUX)
	@echo "Building manylinux2014 wheels..."
	./docker/build.sh manylinux2014

.PHONY: wheels-linux-manylinux_2_28
wheels-linux-manylinux_2_28: $(DIST_LINUX)
	@echo "Building manylinux_2_28 wheels..."
	./docker/build.sh manylinux_2_28

.PHONY: wheels-linux-debian12
wheels-linux-debian12: $(DIST_LINUX)
	@echo "Building Debian 12 wheel..."
	./docker/build.sh debian12

.PHONY: wheels-linux-ubuntu2204
wheels-linux-ubuntu2204: $(DIST_LINUX)
	@echo "Building Ubuntu 22.04 wheel..."
	./docker/build.sh ubuntu2204

.PHONY: wheels-linux-ubuntu2404
wheels-linux-ubuntu2404: $(DIST_LINUX)
	@echo "Building Ubuntu 24.04 wheel..."
	./docker/build.sh ubuntu2404

wheels-linux: wheels-linux-manylinux2014 wheels-linux-manylinux_2_28 \
              wheels-linux-debian12 wheels-linux-ubuntu2204 wheels-linux-ubuntu2404
	@echo ""
	@echo "Linux wheels built:"
	@find $(DIST_LINUX) -name "*.whl" -type f

# ============================================================================
# Combined Builds
# ============================================================================

wheels-all: wheels-macos wheels-linux
	@echo ""
	@echo "========================================"
	@echo "All wheels built successfully!"
	@echo "========================================"
	@echo ""
	@echo "macOS wheels:"
	@ls $(DIST_MACOS)/*.whl 2>/dev/null || echo "  (none)"
	@echo ""
	@echo "Linux wheels:"
	@find $(DIST_LINUX) -name "*.whl" -type f 2>/dev/null || echo "  (none)"

# ============================================================================
# Test & Clean
# ============================================================================

test:
	@echo "Testing mesh2splat import..."
	uv run python -c "import mesh2splat; print(f'Version: {mesh2splat.__version__}'); print(f'Build: {mesh2splat.get_build_info()}')"

clean:
	rm -rf build/
	rm -rf dist/
	rm -rf *.egg-info/
	rm -rf python/mesh2splat/*.so
	rm -rf python/mesh2splat/__pycache__/
	rm -rf wheelhouse/
	@echo "Cleaned build artifacts"

# ============================================================================
# Development helpers
# ============================================================================

.PHONY: dev-env
dev-env:
	@echo "Setting up development environment..."
	@echo ""
	@echo "1. Initializing git submodules..."
	git submodule update --init --recursive
	@echo ""
	@echo "2. Installing Python versions via uv..."
	uv python install 3.10 3.11 3.12
	@echo ""
	@echo "3. Creating virtual environment with uv..."
	uv venv --python 3.12 .venv
	@echo ""
	@echo "4. Installing build dependencies..."
	uv pip install --python .venv/bin/python build scikit-build-core pybind11 numpy pytest
	@echo ""
	@echo "========================================"
	@echo "Development environment ready!"
	@echo "========================================"
	@echo ""
	@echo "Activate with:  source .venv/bin/activate"
	@echo ""
	@echo "Build wheels:   make wheels-macos"
	@echo "Dev install:    make dev-install"
	@echo "Run tests:      make test"

.PHONY: dev-install
dev-install:
	@echo "Installing in development mode..."
	uv pip install -e . -v

.PHONY: sdist
sdist:
	@echo "Building source distribution..."
	uv run python -m build --sdist -o $(DIST_DIR)
