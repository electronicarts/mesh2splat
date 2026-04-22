///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - CPU Converter Header                //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../core/Types.hpp"
#include <memory>

namespace mesh2splat {

/// CPU-based mesh to Gaussian converter
/// This class ties together the Rasterizer and TextureSampler to convert
/// mesh triangles into Gaussian splats using pure CPU computation.
class CPUConverter {
public:
    /// Constructor
    CPUConverter();
    
    /// Destructor
    ~CPUConverter();
    
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
    
    /// Check if CPU backend is available (always true)
    static bool isAvailable() { return true; }
    
    /// Get backend type
    static Backend getBackendType() { return Backend::CPU; }

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mesh2splat
