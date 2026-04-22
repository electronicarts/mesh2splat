///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Texture Sampler Header              //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Types.hpp"

namespace mesh2splat {

/// CPU-based texture sampler for reading texture data
class TextureSampler {
public:
    /// Sample a texture at the given UV coordinates
    /// @param texture Texture data
    /// @param uv UV coordinates (0-1 range, will be clamped)
    /// @return RGBA color (0-1 range)
    static glm::vec4 sample(const TextureData& texture, const glm::vec2& uv);
    
    /// Sample a texture with bilinear interpolation
    /// @param texture Texture data
    /// @param uv UV coordinates (0-1 range)
    /// @return RGBA color (0-1 range)
    static glm::vec4 sampleBilinear(const TextureData& texture, const glm::vec2& uv);
    
    /// Sample a texture at exact pixel coordinates
    /// @param texture Texture data
    /// @param x Pixel x coordinate
    /// @param y Pixel y coordinate
    /// @return RGBA color (0-1 range)
    static glm::vec4 samplePixel(const TextureData& texture, int x, int y);
    
    /// Convert UV to pixel coordinates
    static glm::ivec2 uvToPixel(const glm::vec2& uv, int width, int height);
    
    /// Convert pixel to UV coordinates
    static glm::vec2 pixelToUV(const glm::ivec2& pixel, int width, int height);
};

/// Helper struct for material texture sampling results
struct MaterialSample {
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};
    float metallic = 0.0f;
    float roughness = 1.0f;
    float ao = 1.0f;
    glm::vec3 emissive{0.0f};
};

/// Sample all material textures at a given UV coordinate
class MaterialSampler {
public:
    /// Sample all material properties at UV
    /// @param material Material with textures
    /// @param uv UV coordinates
    /// @param interpolatedNormal Interpolated vertex normal
    /// @param interpolatedTangent Interpolated vertex tangent
    /// @return MaterialSample with all properties
    static MaterialSample sample(const Material& material,
                                 const glm::vec2& uv,
                                 const glm::vec3& interpolatedNormal,
                                 const glm::vec4& interpolatedTangent);
};

} // namespace mesh2splat
