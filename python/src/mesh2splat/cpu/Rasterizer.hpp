///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - CPU Rasterizer Header               //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "../core/Types.hpp"
#include <vector>
#include <functional>

namespace mesh2splat {

/// Per-pixel data during rasterization
struct RasterFragment {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec4 tangent;
    glm::vec2 uv;
    glm::vec3 scale;
    glm::vec4 rotation;
    bool valid = false;
};

/// CPU rasterizer that processes triangles in UV/projection space
class Rasterizer {
public:
    /// Callback type for fragment processing
    using FragmentCallback = std::function<void(const RasterFragment&, const Material&)>;
    
    /// Constructor
    /// @param resolution Resolution of the rasterization grid
    /// @param mode Rasterization mode (UV or Projection)
    explicit Rasterizer(int resolution = 512, RasterizationMode mode = RasterizationMode::UV);
    
    /// Rasterize a mesh and call the callback for each fragment
    /// @param mesh Mesh to rasterize
    /// @param callback Callback called for each valid fragment
    void rasterize(const Mesh& mesh, const FragmentCallback& callback);
    
    /// Rasterize all meshes in a scene
    /// @param scene Scene to rasterize
    /// @param callback Callback called for each valid fragment
    void rasterize(const Scene& scene, const FragmentCallback& callback);
    
    /// Set resolution
    void setResolution(int resolution) { resolution_ = resolution; }
    
    /// Get resolution
    int getResolution() const { return resolution_; }
    
    /// Set rasterization mode
    void setMode(RasterizationMode mode) { mode_ = mode; }
    
    /// Get rasterization mode
    RasterizationMode getMode() const { return mode_; }

private:
    int resolution_;
    RasterizationMode mode_;
    
    /// Compute face-level data for projection mode (scale, rotation, orthogonal UVs)
    /// Port of converterGS.glsl logic
    void computeFaceDataProjection(const Face& face, const BBox& bbox,
                                   glm::vec3& outScale, glm::vec4& outRotation,
                                   std::array<glm::vec2, 3>& outRasterUvs);
    
    /// Compute face-level data for UV mode (scale, rotation from original UVs)
    void computeFaceDataUV(const Face& face,
                           glm::vec3& outScale, glm::vec4& outRotation,
                           std::array<glm::vec2, 3>& outRasterUvs);
    
    /// Rasterize a single triangle
    void rasterizeTriangle(const Face& face, const Material& material,
                           const glm::vec3& scale, const glm::vec4& rotation,
                           const std::array<glm::vec2, 3>& rasterUvs,
                           const FragmentCallback& callback);
    
    /// Compute the Jacobian matrix from UV to 3D space
    glm::mat2x3 computeUv3DJacobian(const std::array<glm::vec3, 3>& positions,
                                     const std::array<glm::vec2, 3>& uvs);
    
    /// Convert rotation matrix to quaternion
    glm::quat matrixToQuaternion(const glm::mat3& m);
};

} // namespace mesh2splat
