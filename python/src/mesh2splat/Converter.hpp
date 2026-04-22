///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Converter Interface Header          //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "core/Types.hpp"
#include <memory>
#include <string>

namespace mesh2splat {

// Forward declarations
class CPUConverter;
class GPUConverter;

/// Abstract converter interface
class IConverter {
public:
    virtual ~IConverter() = default;
    
    /// Convert a scene to Gaussians
    virtual ConversionResult convert(const Scene& scene, const ConversionOptions& options) = 0;
    
    /// Convert a single mesh to Gaussians
    virtual ConversionResult convert(const Mesh& mesh, const ConversionOptions& options) = 0;
    
    /// Get the backend type this converter uses
    virtual Backend getBackendType() const = 0;
};

/// Main converter class with automatic backend selection
/// This is the primary interface for Python bindings
class Converter {
public:
    /// Constructor with backend selection
    /// @param backend Backend to use (Auto, CPU, or GPU)
    explicit Converter(Backend backend = Backend::Auto);
    
    /// Destructor
    ~Converter();
    
    /// Convert a scene to Gaussians
    /// @param scene Scene containing meshes to convert
    /// @param options Conversion options (resolution, format, etc.)
    /// @return ConversionResult with gaussians and statistics
    ConversionResult convert(const Scene& scene, const ConversionOptions& options = {});
    
    /// Convert a single mesh to Gaussians
    /// @param mesh Mesh to convert
    /// @param options Conversion options
    /// @return ConversionResult with gaussians and statistics
    ConversionResult convert(const Mesh& mesh, const ConversionOptions& options = {});
    
    /// Load a GLTF/GLB file and convert to Gaussians
    /// @param path Path to GLTF or GLB file
    /// @param options Conversion options
    /// @return ConversionResult with gaussians and statistics
    ConversionResult convertFile(const std::string& path, const ConversionOptions& options = {});
    
    /// Get the backend currently being used
    Backend getActiveBackend() const;
    
    /// Check if the converter is ready
    bool isReady() const;
    
    /// Get any error message
    std::string getErrorMessage() const;
    
    /// Get available backends on this system
    static std::vector<Backend> getAvailableBackends();
    
    /// Check if a specific backend is available
    static bool isBackendAvailable(Backend backend);

    // Non-copyable
    Converter(const Converter&) = delete;
    Converter& operator=(const Converter&) = delete;
    
    // Movable
    Converter(Converter&& other) noexcept;
    Converter& operator=(Converter&& other) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// Convenience function to convert a file directly
/// @param inputPath Path to input mesh file (GLTF/GLB)
/// @param outputPath Path to output PLY file
/// @param options Conversion options
/// @return true if successful
bool convertMeshToSplat(const std::string& inputPath,
                        const std::string& outputPath,
                        const ConversionOptions& options = {});

/// Convenience function to get library version
std::string getVersion();

/// Convenience function to get build info
std::string getBuildInfo();

} // namespace mesh2splat
