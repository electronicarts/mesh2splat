///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Core Types Implementation           //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "Types.hpp"
#include <cmath>
#include <algorithm>

namespace mesh2splat {

//------------------------------------------------------------------------------
// Gaussian Implementation
//------------------------------------------------------------------------------

Gaussian Gaussian::fromSSBO(const GaussianSSBO& ssbo) {
    Gaussian g;
    
    // Position
    g.x = ssbo.position.x;
    g.y = ssbo.position.y;
    g.z = ssbo.position.z;
    
    // Color (SH0)
    g.r = ssbo.color.x;
    g.g = ssbo.color.y;
    g.b = ssbo.color.z;
    g.opacity = ssbo.color.w;
    
    // Scale - use linearScale
    g.scale_x = ssbo.linearScale.x;
    g.scale_y = ssbo.linearScale.y;
    g.scale_z = ssbo.linearScale.z;
    
    // Normal
    g.nx = ssbo.normal.x;
    g.ny = ssbo.normal.y;
    g.nz = ssbo.normal.z;
    
    // Rotation
    g.rot_x = ssbo.rotation.x;
    g.rot_y = ssbo.rotation.y;
    g.rot_z = ssbo.rotation.z;
    g.rot_w = ssbo.rotation.w;
    
    // PBR
    g.metallic = ssbo.pbr.x;
    g.roughness = ssbo.pbr.y;
    g.ao = ssbo.pbr.z;
    
    return g;
}

GaussianSSBO Gaussian::toSSBO() const {
    GaussianSSBO ssbo;
    
    ssbo.position = glm::vec4(x, y, z, 1.0f);
    ssbo.color = glm::vec4(r, g, b, opacity);
    ssbo.linearScale = glm::vec4(scale_x, scale_y, scale_z, 0.0f);
    ssbo.normal = glm::vec4(nx, ny, nz, 0.0f);
    ssbo.rotation = glm::vec4(rot_x, rot_y, rot_z, rot_w);
    ssbo.pbr = glm::vec4(metallic, roughness, ao, 0.0f);
    
    return ssbo;
}

bool Gaussian::isValid() const {
    // Check for NaN or invalid values
    if (std::isnan(x) || std::isnan(y) || std::isnan(z)) return false;
    if (std::isnan(scale_x) || std::isnan(scale_y) || std::isnan(scale_z)) return false;
    
    // Check for zero or negative scale (invalid gaussian)
    if (scale_x <= 0.0f || scale_y <= 0.0f || scale_z <= 0.0f) return false;
    
    // Check for invalid rotation quaternion
    float rotLen = rot_x*rot_x + rot_y*rot_y + rot_z*rot_z + rot_w*rot_w;
    if (rotLen < 0.9f || rotLen > 1.1f) return false;
    
    return true;
}

//------------------------------------------------------------------------------
// Color Conversion Utilities
//------------------------------------------------------------------------------

// sRGB to linear conversion (based on official sRGB spec)
static float srgbChannelToLinear(float x) {
    if (x <= 0.04045f) {
        return x / 12.92f;
    } else {
        return std::pow((x + 0.055f) / 1.055f, 2.4f);
    }
}

// Linear to sRGB conversion
static float linearChannelToSRGB(float x) {
    if (x <= 0.0031308f) {
        return x * 12.92f;
    } else {
        return 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
    }
}

glm::vec3 linearToSRGB(const glm::vec3& linear) {
    return glm::vec3(
        linearChannelToSRGB(linear.r),
        linearChannelToSRGB(linear.g),
        linearChannelToSRGB(linear.b)
    );
}

glm::vec3 srgbToLinear(const glm::vec3& srgb) {
    return glm::vec3(
        srgbChannelToLinear(srgb.r),
        srgbChannelToLinear(srgb.g),
        srgbChannelToLinear(srgb.b)
    );
}

glm::vec3 colorToSH0(const glm::vec3& color) {
    // Convert normalized color [0,1] to SH0 coefficient
    // SH0 = (color - 0.5) / SH_COEFF0
    return (color - 0.5f) / SH_COEFF0;
}

glm::vec3 sh0ToColor(const glm::vec3& sh0) {
    // Convert SH0 coefficient back to color
    // color = sh0 * SH_COEFF0 + 0.5
    return sh0 * SH_COEFF0 + 0.5f;
}

//------------------------------------------------------------------------------
// Geometry Utilities
//------------------------------------------------------------------------------

bool computeBarycentric(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c,
                        float& u, float& v, float& w) {
    glm::vec2 v0 = b - a;
    glm::vec2 v1 = c - a;
    glm::vec2 v2 = p - a;
    
    float d00 = glm::dot(v0, v0);
    float d01 = glm::dot(v0, v1);
    float d11 = glm::dot(v1, v1);
    float d20 = glm::dot(v2, v0);
    float d21 = glm::dot(v2, v1);
    
    float denom = d00 * d11 - d01 * d01;
    if (std::abs(denom) < 1e-10f) {
        return false;  // Degenerate triangle
    }
    
    v = (d11 * d20 - d01 * d21) / denom;
    w = (d00 * d21 - d01 * d20) / denom;
    u = 1.0f - v - w;
    
    return true;
}

bool pointInTriangle(const glm::vec2& p, const glm::vec2& v1, const glm::vec2& v2, const glm::vec2& v3) {
    float u, v, w;
    if (!computeBarycentric(p, v1, v2, v3, u, v, w)) {
        return false;
    }
    return (u >= 0.0f) && (v >= 0.0f) && (w >= 0.0f);
}

float triangleArea3D(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    return 0.5f * glm::length(glm::cross(b - a, c - a));
}

float triangleAreaUV(const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3) {
    // Shoelace formula for triangle area in 2D
    return 0.5f * std::abs(
        (uv2.x - uv1.x) * (uv3.y - uv1.y) - 
        (uv3.x - uv1.x) * (uv2.y - uv1.y)
    );
}

//------------------------------------------------------------------------------
// Backend Utilities
//------------------------------------------------------------------------------

std::string getBackendName(Backend backend) {
    switch (backend) {
        case Backend::Auto: return "Auto";
        case Backend::CPU: return "CPU";
        case Backend::GPU: return "GPU";
        default: return "Unknown";
    }
}

std::vector<Backend> getAvailableBackends() {
    std::vector<Backend> backends;
    backends.push_back(Backend::CPU);  // CPU is always available
    
#ifdef MESH2SPLAT_ENABLE_GPU
    backends.push_back(Backend::GPU);
#endif
    
    return backends;
}

//------------------------------------------------------------------------------
// Color/Opacity Encoding Helpers
//------------------------------------------------------------------------------

glm::vec3 computeDcFromColor(const glm::vec3& colorLinear, DcMode dcMode) {
    switch (dcMode) {
        case DcMode::Current:
            // SH0 encoding: (color - 0.5) / SH_COEFF0
            return colorToSH0(colorLinear);
        case DcMode::DirectLinear:
            // Linear RGB directly
            return colorLinear;
        case DcMode::DirectSrgb:
            // sRGB values directly
            return linearToSRGB(colorLinear);
        default:
            return colorToSH0(colorLinear);
    }
}

float encodeOpacity(float opacity, OpacityMode mode) {
    switch (mode) {
        case OpacityMode::Current:
        case OpacityMode::Logit:
            // Inverse sigmoid (logit) - standard for PLY files
            opacity = glm::clamp(opacity, 0.0001f, 0.9999f);
            return std::log(opacity / (1.0f - opacity));
        case OpacityMode::Raw:
            // Raw opacity (0-1)
            return opacity;
        default:
            opacity = glm::clamp(opacity, 0.0001f, 0.9999f);
            return std::log(opacity / (1.0f - opacity));
    }
}

float safeLog(float v) {
    return std::log(std::max(v, 1e-12f));
}

} // namespace mesh2splat
