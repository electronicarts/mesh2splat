# Python bindings for Mesh2Splat
cmake_minimum_required(VERSION 3.15)

set(PYBIND11_DIR ${CMAKE_SOURCE_DIR}/thirdParty/pybind11)

# Find Python
find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)

# Source files for the binding library
set(MESH2SPLAT_PY_SOURCES
    src/mesh2splat/core/Types.cpp
    src/mesh2splat/core/GltfLoader.cpp
    src/mesh2splat/core/PlyIO.cpp
    src/mesh2splat/core/TextureSampler.cpp
    src/mesh2splat/cpu/Rasterizer.cpp
    src/mesh2splat/cpu/CPUConverter.cpp
    src/mesh2splat/Converter.cpp
    src/mesh2splat/Bindings.cpp
)

# GPU sources (conditional)
if(MESH2SPLAT_ENABLE_GPU)
    list(APPEND MESH2SPLAT_PY_SOURCES
        src/mesh2splat/gpu/HeadlessContext.cpp
        src/mesh2splat/gpu/ShaderManager.cpp
        src/mesh2splat/gpu/GPUConverter.cpp
        src/mesh2splat/gpu/GLLoader.cpp
    )
endif()

# Create the Python module
pybind11_add_module(_mesh2splat MODULE ${MESH2SPLAT_PY_SOURCES})

# Include directories
target_include_directories(_mesh2splat PRIVATE
    ${CMAKE_SOURCE_DIR}/thirdParty        # For stb_image, tiny_gltf, happly, glm
    ${CMAKE_SOURCE_DIR}/thirdParty/glm
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile definitions
if(MESH2SPLAT_ENABLE_GPU)
    target_compile_definitions(_mesh2splat PRIVATE MESH2SPLAT_ENABLE_GPU)
endif()

# Platform-specific GPU setup
if(MESH2SPLAT_ENABLE_GPU)
    if(APPLE)
        # macOS: Use CGL for headless OpenGL context
        find_library(COCOA_LIBRARY Cocoa REQUIRED)
        find_library(OPENGL_LIBRARY OpenGL REQUIRED)
        find_library(IOKIT_LIBRARY IOKit REQUIRED)
        find_library(COREVIDEO_LIBRARY CoreVideo REQUIRED)
        target_link_libraries(_mesh2splat PRIVATE 
            ${COCOA_LIBRARY} 
            ${OPENGL_LIBRARY}
            ${IOKIT_LIBRARY}
            ${COREVIDEO_LIBRARY}
        )
        target_compile_definitions(_mesh2splat PRIVATE MESH2SPLAT_USE_CGL)
    elseif(UNIX)
        # Linux: Use EGL for headless OpenGL context
        find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)
        target_link_libraries(_mesh2splat PRIVATE OpenGL::OpenGL OpenGL::EGL)
        target_compile_definitions(_mesh2splat PRIVATE MESH2SPLAT_USE_EGL)
    elseif(WIN32)
        # Windows: Use WGL (not yet implemented, GPU disabled on Windows by default)
        find_package(OpenGL REQUIRED)
        target_link_libraries(_mesh2splat PRIVATE OpenGL::GL)
        target_compile_definitions(_mesh2splat PRIVATE MESH2SPLAT_USE_WGL)
    endif()
endif()

# Static linking for portable Linux wheels (avoid GLIBC/GLIBCXX version issues)
if(UNIX AND NOT APPLE)
    target_link_options(_mesh2splat PRIVATE -static-libstdc++ -static-libgcc)
endif()

# For local development: output to python package directory
if(NOT MESH2SPLAT_STANDALONE_PYTHON)
    set_target_properties(_mesh2splat PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mesh2splat
    )
endif()

# For pip install: install the module to the correct location
# scikit-build-core will place these at the root of the wheel
install(TARGETS _mesh2splat LIBRARY DESTINATION .)

# Install the Python package files
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/mesh2splat/__init__.py
        DESTINATION .)
