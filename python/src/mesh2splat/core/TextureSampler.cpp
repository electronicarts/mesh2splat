///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Texture Sampler Implementation      //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "TextureSampler.hpp"
#include <cmath>
#include <algorithm>

namespace mesh2splat {

glm::ivec2 TextureSampler::uvToPixel(const glm::vec2& uv, int width, int height) {
    // UV coordinates are typically in [0,1] range
    // Clamp to valid range
    float u = glm::clamp(uv.x, 0.0f, 1.0f);
    float v = glm::clamp(uv.y, 0.0f, 1.0f);
    
    // Convert to pixel coordinates
    int x = static_cast<int>(u * (width - 1));
    int y = static_cast<int>(v * (height - 1));
    
    return glm::ivec2(x, y);
}

glm::vec2 TextureSampler::pixelToUV(const glm::ivec2& pixel, int width, int height) {
    float u = static_cast<float>(pixel.x) / static_cast<float>(width - 1);
    float v = static_cast<float>(pixel.y) / static_cast<float>(height - 1);
    return glm::vec2(u, v);
}

glm::vec4 TextureSampler::samplePixel(const TextureData& texture, int x, int y) {
    if (texture.empty()) {
        return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    // Clamp coordinates
    x = glm::clamp(x, 0, texture.width - 1);
    y = glm::clamp(y, 0, texture.height - 1);
    
    size_t index = (y * texture.width + x) * texture.channels;
    
    glm::vec4 color(1.0f);
    
    if (texture.channels >= 1) {
        color.r = texture.data[index + 0] / 255.0f;
    }
    if (texture.channels >= 2) {
        color.g = texture.data[index + 1] / 255.0f;
    }
    if (texture.channels >= 3) {
        color.b = texture.data[index + 2] / 255.0f;
    }
    if (texture.channels >= 4) {
        color.a = texture.data[index + 3] / 255.0f;
    } else {
        color.a = 1.0f;
    }
    
    return color;
}

glm::vec4 TextureSampler::sample(const TextureData& texture, const glm::vec2& uv) {
    if (texture.empty()) {
        return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    glm::ivec2 pixel = uvToPixel(uv, texture.width, texture.height);
    return samplePixel(texture, pixel.x, pixel.y);
}

glm::vec4 TextureSampler::sampleBilinear(const TextureData& texture, const glm::vec2& uv) {
    if (texture.empty()) {
        return glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    
    // Clamp UV to valid range
    float u = glm::clamp(uv.x, 0.0f, 1.0f);
    float v = glm::clamp(uv.y, 0.0f, 1.0f);
    
    // Convert to floating point pixel coordinates
    float px = u * (texture.width - 1);
    float py = v * (texture.height - 1);
    
    // Get integer coordinates
    int x0 = static_cast<int>(std::floor(px));
    int y0 = static_cast<int>(std::floor(py));
    int x1 = std::min(x0 + 1, texture.width - 1);
    int y1 = std::min(y0 + 1, texture.height - 1);
    
    // Get fractional parts
    float fx = px - x0;
    float fy = py - y0;
    
    // Sample four corners
    glm::vec4 c00 = samplePixel(texture, x0, y0);
    glm::vec4 c10 = samplePixel(texture, x1, y0);
    glm::vec4 c01 = samplePixel(texture, x0, y1);
    glm::vec4 c11 = samplePixel(texture, x1, y1);
    
    // Bilinear interpolation
    glm::vec4 c0 = glm::mix(c00, c10, fx);
    glm::vec4 c1 = glm::mix(c01, c11, fx);
    
    return glm::mix(c0, c1, fy);
}

//------------------------------------------------------------------------------
// MaterialSampler Implementation
//------------------------------------------------------------------------------

MaterialSample MaterialSampler::sample(const Material& material,
                                       const glm::vec2& uv,
                                       const glm::vec3& interpolatedNormal,
                                       const glm::vec4& interpolatedTangent) {
    MaterialSample result;
    
    // Base color
    if (!material.baseColorTexture.empty()) {
        result.baseColor = TextureSampler::sampleBilinear(material.baseColorTexture, uv);
        result.baseColor *= material.baseColorFactor;
    } else {
        result.baseColor = material.baseColorFactor;
    }
    
    // Metallic-Roughness (GLTF: G=roughness, B=metallic)
    if (!material.metallicRoughnessTexture.empty()) {
        glm::vec4 mrSample = TextureSampler::sampleBilinear(material.metallicRoughnessTexture, uv);
        result.roughness = mrSample.g * material.roughnessFactor;
        result.metallic = mrSample.b * material.metallicFactor;
    } else {
        result.roughness = material.roughnessFactor;
        result.metallic = material.metallicFactor;
    }
    
    // Ambient Occlusion (typically in R channel of occlusion texture)
    if (!material.occlusionTexture.empty()) {
        glm::vec4 aoSample = TextureSampler::sampleBilinear(material.occlusionTexture, uv);
        result.ao = aoSample.r * material.occlusionStrength;
    } else {
        result.ao = 1.0f;
    }
    
    // Emissive
    if (!material.emissiveTexture.empty()) {
        glm::vec4 emissiveSample = TextureSampler::sampleBilinear(material.emissiveTexture, uv);
        result.emissive = glm::vec3(emissiveSample) * material.emissiveFactor;
    } else {
        result.emissive = material.emissiveFactor;
    }
    
    // Normal mapping
    if (!material.normalTexture.empty()) {
        // Sample normal map (stored in tangent space)
        glm::vec4 normalSample = TextureSampler::sampleBilinear(material.normalTexture, uv);
        
        // Convert from [0,1] to [-1,1]
        glm::vec3 tangentNormal;
        tangentNormal.x = normalSample.r * 2.0f - 1.0f;
        tangentNormal.y = normalSample.g * 2.0f - 1.0f;
        tangentNormal.z = normalSample.b * 2.0f - 1.0f;
        
        // Apply normal scale
        tangentNormal.x *= material.normalScale;
        tangentNormal.y *= material.normalScale;
        
        // Normalize
        tangentNormal = glm::normalize(tangentNormal);
        
        // Compute TBN matrix
        glm::vec3 N = glm::normalize(interpolatedNormal);
        glm::vec3 T = glm::normalize(glm::vec3(interpolatedTangent));
        
        // Re-orthogonalize T with respect to N
        T = glm::normalize(T - glm::dot(T, N) * N);
        
        // Calculate bitangent
        glm::vec3 B = glm::cross(N, T) * interpolatedTangent.w;
        
        // Transform normal from tangent space to world space
        glm::mat3 TBN(T, B, N);
        result.normal = glm::normalize(TBN * tangentNormal);
    } else {
        result.normal = glm::normalize(interpolatedNormal);
    }
    
    return result;
}

} // namespace mesh2splat
