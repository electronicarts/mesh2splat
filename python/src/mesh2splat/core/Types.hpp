///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Core Types                          //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace mesh2splat {

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

/// Spherical harmonics coefficient for degree 0
constexpr float SH_COEFF0 = 0.28209479177387814f;

/// Epsilon values for numerical stability
namespace epsilon {
    /// Default epsilon for degenerate triangle detection
    constexpr float kDegenerate = 1e-8f;
    /// Minimum scale value for gaussians (avoids zero-size)
    constexpr float kMinScale = 1e-7f;
    /// Epsilon for matrix determinant checks
    constexpr float kDeterminant = 1e-10f;
    /// Epsilon for quaternion sqrt clamping
    constexpr float kQuatSqrt = 1e-10f;
    /// Epsilon for range normalization
    constexpr float kRange = 1e-8f;
    /// Small value to avoid division by zero in normalization
    constexpr float kNormalize = 1e-8f;
} // namespace epsilon

//------------------------------------------------------------------------------
// Enums
//------------------------------------------------------------------------------

enum class Backend {
    Auto,   // Try GPU first, fallback to CPU
    CPU,
    GPU
};

enum class PlyFormat {
    Standard = 0,   // Basic 3DGS format
    PBR = 1,        // With metallic/roughness/AO
    Compressed = 2  // Compressed format
};

enum class RasterizationMode {
    UV,             // Rasterize in original mesh UV space (texture-based)
    Projection      // Rasterize using orthogonal projection (triplanar)
};

/// DC (color) encoding mode
enum class DcMode {
    Current = 0,      // SH0 encoding (default, matches original behavior)
    DirectLinear = 1, // Linear RGB directly
    DirectSrgb = 2    // sRGB values directly
};

/// Opacity encoding mode
enum class OpacityMode {
    Current = 0,  // Format-specific default
    Raw = 1,      // Raw opacity (0-1)
    Logit = 2     // Inverse sigmoid (standard for PLY)
};

//------------------------------------------------------------------------------
// Core Data Structures
//------------------------------------------------------------------------------

/// Texture information with pixel data
struct TextureData {
    std::vector<uint8_t> data;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string path;
    int texCoordIndex = 0;
    
    bool empty() const { return data.empty() || width == 0 || height == 0; }
};

/// Material properties (PBR workflow)
struct Material {
    std::string name = "Default";
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    TextureData baseColorTexture;
    TextureData normalTexture;
    TextureData metallicRoughnessTexture;
    TextureData occlusionTexture;
    TextureData emissiveTexture;
    float metallicFactor = 1.0f;
    float roughnessFactor = 1.0f;
    float occlusionStrength = 1.0f;
    float normalScale = 1.0f;
    glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f};  // glTF spec default: no emission
};

/// Triangle face with all vertex attributes
struct Face {
    std::array<glm::vec3, 3> positions{};
    std::array<glm::vec2, 3> uvs{};
    std::array<glm::vec2, 3> normalizedUvs{};  // From xatlas
    std::array<glm::vec3, 3> normals{};
    std::array<glm::vec4, 3> tangents{};
    glm::vec3 scale{1.0f};
    glm::vec4 rotation{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion (w,x,y,z)
};

/// Axis-aligned bounding box
struct BBox {
    glm::vec3 min{0.0f};
    glm::vec3 max{0.0f};
    
    glm::vec3 center() const { return (min + max) * 0.5f; }
    glm::vec3 size() const { return max - min; }
};

/// Mesh primitive (submesh with single material)
struct Mesh {
    std::string name = "Unnamed";
    std::vector<Face> faces;
    Material material;
    float surfaceArea = 0.0f;
    BBox bbox;
};

/// Complete scene with multiple meshes
struct Scene {
    std::vector<Mesh> meshes;
    BBox bbox;
    std::string sourcePath;
};

//------------------------------------------------------------------------------
// Gaussian Data Structures
//------------------------------------------------------------------------------

/// Single Gaussian splat (matches GPU SSBO layout: 6 × vec4 = 24 floats)
struct alignas(16) GaussianSSBO {
    glm::vec4 position;    // xyz = position, w = unused
    glm::vec4 color;       // rgb = SH0 coefficients, a = opacity
    glm::vec4 linearScale; // xyz = linear scale (renamed from 'scale'), w = unused
    glm::vec4 normal;      // xyz = normal, w = unused
    glm::vec4 rotation;    // xyzw = quaternion
    glm::vec4 pbr;         // x = metallic, y = roughness, z = ao, w = unused
};

static_assert(sizeof(GaussianSSBO) == 96, "GaussianSSBO must be 96 bytes (6 × vec4)");

/// Python-friendly Gaussian representation
struct Gaussian {
    // Position
    float x = 0.0f, y = 0.0f, z = 0.0f;
    
    // Color (SH0 coefficients)
    float r = 0.0f, g = 0.0f, b = 0.0f;
    float opacity = 1.0f;
    
    // Scale
    float scale_x = 1.0f, scale_y = 1.0f, scale_z = 1.0f;
    
    // Rotation (quaternion)
    float rot_x = 0.0f, rot_y = 0.0f, rot_z = 0.0f, rot_w = 1.0f;
    
    // Normal
    float nx = 0.0f, ny = 0.0f, nz = 1.0f;
    
    // PBR
    float metallic = 0.0f;
    float roughness = 1.0f;
    float ao = 1.0f;
    
    /// Convert from SSBO format
    static Gaussian fromSSBO(const GaussianSSBO& ssbo);
    
    /// Convert to SSBO format
    GaussianSSBO toSSBO() const;
    
    /// Check if this gaussian should be skipped (invalid)
    bool isValid() const;
};

//------------------------------------------------------------------------------
// Conversion Options and Results
//------------------------------------------------------------------------------

struct ConversionOptions {
    // Resolution for UV space rasterization
    int resolution = 512;
    
    // Output format
    PlyFormat plyFormat = PlyFormat::Standard;
    
    // Scale multiplier for gaussian scales
    float scaleMultiplier = 1.0f;
    
    // Whether to use sRGB color space conversion
    bool srgbConversion = true;
    
    // Backend selection
    Backend backend = Backend::Auto;
    
    // Rasterization mode (UV-based or projection-based)
    RasterizationMode rasterizationMode = RasterizationMode::UV;
    
    // Color encoding mode
    DcMode dcMode = DcMode::Current;
    
    // Opacity encoding mode
    OpacityMode opacityMode = OpacityMode::Logit;
    
    // Flip Y axis for SuperSplat compatibility (180° rotation around X axis)
    bool flipY = true;
    
    // Verbose logging
    bool verbose = false;
};

struct ConversionResult {
    std::vector<Gaussian> gaussians;
    
    // Statistics
    size_t totalTriangles = 0;
    size_t totalGaussians = 0;
    double conversionTimeMs = 0.0;
    
    // Backend that was used
    Backend usedBackend = Backend::CPU;
    
    // Any warnings or info messages
    std::vector<std::string> messages;
    
    bool success = false;
    std::string errorMessage;
};

//------------------------------------------------------------------------------
// Utility Functions
//------------------------------------------------------------------------------

/// Convert linear RGB to sRGB
glm::vec3 linearToSRGB(const glm::vec3& linear);

/// Convert sRGB to linear RGB
glm::vec3 srgbToLinear(const glm::vec3& srgb);

/// Convert color to SH0 coefficient
glm::vec3 colorToSH0(const glm::vec3& color);

/// Convert SH0 coefficient to color
glm::vec3 sh0ToColor(const glm::vec3& sh0);

/// Compute barycentric coordinates for point p in triangle abc
bool computeBarycentric(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b, const glm::vec2& c,
                        float& u, float& v, float& w);

/// Test if point is inside triangle
bool pointInTriangle(const glm::vec2& p, const glm::vec2& v1, const glm::vec2& v2, const glm::vec2& v3);

/// Compute triangle area in 3D space
float triangleArea3D(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

/// Compute triangle area in UV space
float triangleAreaUV(const glm::vec2& uv1, const glm::vec2& uv2, const glm::vec2& uv3);

/// Get backend name as string
std::string getBackendName(Backend backend);

/// Get available backends on this system
std::vector<Backend> getAvailableBackends();

/// Compute DC (color) from linear color based on mode
glm::vec3 computeDcFromColor(const glm::vec3& colorLinear, DcMode dcMode);

/// Encode opacity based on mode
float encodeOpacity(float opacity, OpacityMode mode);

/// Safe log function (avoids -inf for very small values)
float safeLog(float v);

} // namespace mesh2splat
