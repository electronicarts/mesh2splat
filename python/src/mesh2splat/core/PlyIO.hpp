///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - PLY I/O Header                      //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Types.hpp"
#include <string>
#include <vector>

namespace mesh2splat {

/// PLY file I/O for Gaussian splat data
class PlyIO {
public:
    /// Save gaussians to PLY file
    /// @param filename Output PLY file path
    /// @param gaussians Vector of gaussians to save
    /// @param format PLY format (Standard, PBR, Compressed)
    /// @param scaleMultiplier Scale factor for gaussian sizes
    /// @param flipY Flip Y axis for SuperSplat compatibility (180° rotation around X axis)
    static void save(const std::string& filename,
                     const std::vector<Gaussian>& gaussians,
                     PlyFormat format = PlyFormat::Standard,
                     float scaleMultiplier = 1.0f,
                     bool flipY = true);
    
    /// Load gaussians from PLY file
    /// @param filename Input PLY file path
    /// @return Vector of loaded gaussians
    static std::vector<Gaussian> load(const std::string& filename);
    
private:
    /// Write standard 3DGS PLY format (with f_rest_0..f_rest_44)
    static void writeStandard(const std::string& filename,
                              const std::vector<Gaussian>& gaussians,
                              float scaleMultiplier,
                              bool flipY);
    
    /// Write PBR format (with metallic/roughness)
    static void writePBR(const std::string& filename,
                         const std::vector<Gaussian>& gaussians,
                         float scaleMultiplier,
                         bool flipY);
    
    /// Write compressed format (uint8 colors, octahedral normals)
    static void writeCompressed(const std::string& filename,
                                const std::vector<Gaussian>& gaussians,
                                float scaleMultiplier,
                                bool flipY);
    
    /// Encode normal to octahedral representation
    static glm::vec2 encodeOctahedral(const glm::vec3& normal);
};

} // namespace mesh2splat
