///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - GPU Converter Header                //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../core/Types.hpp"
#include <memory>

namespace mesh2splat {

/// GPU-based mesh to Gaussian converter using OpenGL
/// Uses headless rendering with multiple render targets (MRT) to generate gaussians
class GPUConverter {
public:
    /// Constructor
    GPUConverter();
    
    /// Destructor
    ~GPUConverter();
    
    /// Initialize the GPU converter (creates context and compiles shaders)
    /// @return true if successful
    bool initialize();
    
    /// Check if initialized
    bool isInitialized() const;
    
    /// Convert a scene to Gaussians
    /// @param scene Scene containing meshes to convert
    /// @param options Conversion options
    /// @return ConversionResult with gaussians and statistics
    ConversionResult convert(const Scene& scene, const ConversionOptions& options);
    
    /// Convert a single mesh to Gaussians
    /// @param mesh Mesh to convert
    /// @param options Conversion options
    /// @return ConversionResult with gaussians and statistics
    ConversionResult convert(const Mesh& mesh, const ConversionOptions& options);
    
    /// Check if GPU backend is available on this system
    static bool isAvailable();
    
    /// Get backend type
    static Backend getBackendType() { return Backend::GPU; }
    
    /// Get any error message from initialization
    std::string getErrorMessage() const;

    // Non-copyable
    GPUConverter(const GPUConverter&) = delete;
    GPUConverter& operator=(const GPUConverter&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mesh2splat
