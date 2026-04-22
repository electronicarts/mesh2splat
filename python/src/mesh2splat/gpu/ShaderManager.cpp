///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Shader Manager Implementation       //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "ShaderManager.hpp"

// Use our GL loader which handles platform differences
#include "GLLoader.hpp"

#include <vector>
#include <sstream>

namespace mesh2splat {

//------------------------------------------------------------------------------
// Embedded Shader Sources (OpenGL 4.1 compatible for macOS)
//------------------------------------------------------------------------------

namespace shaders {

const char* converterVS = R"GLSL(
#version 410 core

// Input attributes
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec2 normalizedUv;
layout(location = 5) in vec3 scale;

// Output struct
out VS_OUT {
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
    vec2 normalizedUv;
    vec3 scale;
} vs_out;

void main() {
    vs_out.position = position;
    vs_out.normal = normal;
    vs_out.tangent = tangent;
    vs_out.uv = uv;
    vs_out.normalizedUv = normalizedUv;
    vs_out.scale = scale;
}
)GLSL";

const char* converterGS = R"GLSL(
#version 410 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

uniform vec3 u_bboxMin;
uniform vec3 u_bboxMax;
uniform int u_rasterizationMode; // 0 = UV, 1 = Projection

in VS_OUT {
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
    vec2 normalizedUv;
    vec3 scale;
} gs_in[];

out vec3 Position;
flat out vec3 Scale;
out vec2 UV;
out vec4 Tangent;
out vec3 Normal;
flat out vec4 Quaternion;

mat2 inverse2x2(mat2 m) {
    float determinant = m[0][0] * m[1][1] - m[0][1] * m[1][0];
    if (determinant == 0.0) {
        return mat2(0.0);
    }
    float invDet = 1.0 / determinant;
    mat2 inverse;
    inverse[0][0] = m[1][1] * invDet;
    inverse[1][0] = -m[1][0] * invDet;
    inverse[0][1] = -m[0][1] * invDet;
    inverse[1][1] = m[0][0] * invDet;
    return inverse;
}

mat2x3 multiplyMat2x3WithMat2x2(mat2x3 matA, mat2 matB) {
    mat2x3 result;
    result[0][0] = matA[0][0] * matB[0][0] + matA[1][0] * matB[0][1];
    result[1][0] = matA[0][0] * matB[1][0] + matA[1][0] * matB[1][1];
    result[0][1] = matA[0][1] * matB[0][0] + matA[1][1] * matB[0][1];
    result[1][1] = matA[0][1] * matB[1][0] + matA[1][1] * matB[1][1];
    result[0][2] = matA[0][2] * matB[0][0] + matA[1][2] * matB[0][1];
    result[1][2] = matA[0][2] * matB[1][0] + matA[1][2] * matB[1][1];
    return result;
}

mat2x3 computeUv3DJacobian(vec3 pos0, vec3 pos1, vec3 pos2, vec2 uv0, vec2 uv1, vec2 uv2) {
    mat2 UVMatrix;
    UVMatrix[0][0] = uv1.x - uv0.x;
    UVMatrix[1][0] = uv2.x - uv0.x;
    UVMatrix[0][1] = uv1.y - uv0.y;
    UVMatrix[1][1] = uv2.y - uv0.y;

    mat2x3 VMatrix;
    VMatrix[0][0] = pos1.x - pos0.x;
    VMatrix[1][0] = pos2.x - pos0.x;
    VMatrix[0][1] = pos1.y - pos0.y;
    VMatrix[1][1] = pos2.y - pos0.y;
    VMatrix[0][2] = pos1.z - pos0.z;
    VMatrix[1][2] = pos2.z - pos0.z;

    return multiplyMat2x3WithMat2x2(VMatrix, inverse2x2(UVMatrix));
}

vec4 quat_cast(mat3 m) {
    float fourXSquaredMinus1 = m[0][0] - m[1][1] - m[2][2];
    float fourYSquaredMinus1 = m[1][1] - m[0][0] - m[2][2];
    float fourZSquaredMinus1 = m[2][2] - m[0][0] - m[1][1];
    float fourWSquaredMinus1 = m[0][0] + m[1][1] + m[2][2];

    int biggestIndex = 0;
    float fourBiggestSquaredMinus1 = fourWSquaredMinus1;
    if (fourXSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourXSquaredMinus1;
        biggestIndex = 1;
    }
    if (fourYSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourYSquaredMinus1;
        biggestIndex = 2;
    }
    if (fourZSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourZSquaredMinus1;
        biggestIndex = 3;
    }

    float biggestVal = sqrt(fourBiggestSquaredMinus1 + 1.0) * 0.5;
    float mult = 0.25 / biggestVal;

    vec4 q;
    if (biggestIndex == 0) {
        q.w = biggestVal;
        q.x = (m[1][2] - m[2][1]) * mult;
        q.y = (m[2][0] - m[0][2]) * mult;
        q.z = (m[0][1] - m[1][0]) * mult;
    } else if (biggestIndex == 1) {
        q.w = (m[1][2] - m[2][1]) * mult;
        q.x = biggestVal;
        q.y = (m[0][1] + m[1][0]) * mult;
        q.z = (m[2][0] + m[0][2]) * mult;
    } else if (biggestIndex == 2) {
        q.w = (m[2][0] - m[0][2]) * mult;
        q.x = (m[0][1] + m[1][0]) * mult;
        q.y = biggestVal;
        q.z = (m[1][2] + m[2][1]) * mult;
    } else {
        q.w = (m[0][1] - m[1][0]) * mult;
        q.x = (m[2][0] + m[0][2]) * mult;
        q.y = (m[1][2] + m[2][1]) * mult;
        q.z = biggestVal;
    }
    return q;
}

void main() {
    vec3 edge1 = gs_in[1].position - gs_in[0].position;
    vec3 edge2 = gs_in[2].position - gs_in[0].position;
    vec3 edge3 = gs_in[2].position - gs_in[1].position;

    // Compute normal from original edges BEFORE any edge swapping (H14 fix)
    vec3 normal = normalize(cross(edge1, edge2));

    // Find longest edge
    if (length(edge2) > length(edge1) && length(edge2) > length(edge3)) {
        vec3 temp = edge1;
        edge1 = edge2;
        edge2 = temp;
    } else if (length(edge3) > length(edge1) && length(edge3) > length(edge2)) {
        vec3 temp = edge1;
        edge1 = edge3;
        edge3 = temp;
    }

    edge1 = normalize(edge1);

    // Compute rasterization UVs based on mode
    vec2 rasterUvs[3];
    
    if (u_rasterizationMode == 0) {
        // UV mode: use original mesh UVs
        for (int i = 0; i < 3; i++) {
            rasterUvs[i] = gs_in[i].uv;
        }
    } else {
        // Projection mode: compute orthogonal UVs based on face normal
        float absX = abs(normal.x);
        float absY = abs(normal.y);
        float absZ = abs(normal.z);

        for (int i = 0; i < 3; i++) {
            float u, v;
            vec3 pos = gs_in[i].position;

            if (absX > absY && absX > absZ) {
                float rangeY = u_bboxMax.y - u_bboxMin.y;
                float rangeZ = u_bboxMax.z - u_bboxMin.z;
                float range = max(rangeY, rangeZ);
                u = (pos.y - u_bboxMin.y) / range;
                v = (pos.z - u_bboxMin.z) / range;
            } else if (absY > absZ) {
                float rangeX = u_bboxMax.x - u_bboxMin.x;
                float rangeZ = u_bboxMax.z - u_bboxMin.z;
                float range = max(rangeX, rangeZ);
                u = (pos.x - u_bboxMin.x) / range;
                v = (pos.z - u_bboxMin.z) / range;
            } else {
                float rangeX = u_bboxMax.x - u_bboxMin.x;
                float rangeY = u_bboxMax.y - u_bboxMin.y;
                float range = max(rangeX, rangeY);
                u = (pos.x - u_bboxMin.x) / range;
                v = (pos.y - u_bboxMin.y) / range;
            }
            rasterUvs[i] = vec2(u, v);
        }
    }

    vec3 xAxis = edge1;
    vec3 yAxis = normalize(cross(normal, xAxis));
    vec3 zAxis = normal;

    mat3 rotationMatrix = mat3(xAxis, yAxis, zAxis);
    vec4 q = quat_cast(rotationMatrix);
    vec4 quaternion = vec4(q.w, q.x, q.y, q.z);

    // Compute Jacobian using the rasterization UVs for scale computation
    mat2x3 J = computeUv3DJacobian(
        gs_in[0].position, gs_in[1].position, gs_in[2].position,
        rasterUvs[0], rasterUvs[1], rasterUvs[2]);

    vec3 Ju = vec3(J[0][0], J[0][1], J[0][2]);
    vec3 Jv = vec3(J[1][0], J[1][1], J[1][2]);

    float gaussian_scale_x = length(Ju);
    float gaussian_scale_y = length(Jv);

    vec3 computedScale = vec3(gaussian_scale_x, gaussian_scale_y, 1e-7);

    for (int i = 0; i < 3; i++) {
        Tangent = gs_in[i].tangent;
        Position = gs_in[i].position;
        Normal = gs_in[i].normal;
        UV = gs_in[i].uv;  // Always pass original UVs for texture sampling
        Scale = computedScale;
        Quaternion = quaternion;
        gl_Position = vec4(rasterUvs[i] * 2.0 - 1.0, 0.0, 1.0);
        EmitVertex();
    }
    EndPrimitive();
}
)GLSL";

// Fragment shader for OpenGL 4.1 - uses transform feedback instead of SSBO
// Note: On macOS, we use a different approach - render to texture and read back
const char* converterFS = R"GLSL(
#version 410 core

uniform sampler2D albedoTexture;
uniform sampler2D normalTexture;
uniform sampler2D metallicRoughnessTexture;

uniform int hasAlbedoMap;
uniform int hasNormalMap;
uniform int hasMetallicRoughnessMap;
uniform vec4 u_materialFactor;

in vec3 Position;
flat in vec3 Scale;
in vec2 UV;
in vec4 Tangent;
in vec3 Normal;
flat in vec4 Quaternion;

// Output to multiple render targets (MRT) instead of SSBO
layout(location = 0) out vec4 out_Position;   // xyz = position, w = 1
layout(location = 1) out vec4 out_Color;      // rgba = color with opacity
layout(location = 2) out vec4 out_Scale;      // xyz = scale, w = 0
layout(location = 3) out vec4 out_Normal;     // xyz = normal, w = 0
layout(location = 4) out vec4 out_Rotation;   // xyzw = quaternion
layout(location = 5) out vec4 out_PBR;        // x = metallic, y = roughness, z = ao, w = 1

void main() {
    // Base color
    vec4 baseColor;
    if (hasAlbedoMap == 1) {
        baseColor = texture(albedoTexture, UV);
    } else {
        baseColor = vec4(1.0);
    }

    // Discard fully transparent pixels
    if (baseColor.a < 0.01) {
        discard;
    }

    // Normal mapping
    vec3 finalNormal;
    if (hasNormalMap == 1) {
        vec3 normalMap = texture(normalTexture, UV).xyz;
        vec3 retrievedNormal = normalize(normalMap * 2.0 - 1.0);
        vec3 bitangent = normalize(cross(Normal, Tangent.xyz)) * Tangent.w;
        mat3 TBN = mat3(Tangent.xyz, bitangent, normalize(Normal));
        finalNormal = normalize(TBN * retrievedNormal);
    } else {
        finalNormal = normalize(Normal);
    }

    // Metallic-roughness
    vec2 metallicRoughness;
    if (hasMetallicRoughnessMap == 1) {
        vec2 metalRough = texture(metallicRoughnessTexture, UV).bg;
        metallicRoughness = metalRough;
    } else {
        metallicRoughness = vec2(0.1, 0.5);
    }

    // Output to render targets
    out_Position = vec4(Position, 1.0);
    out_Color = baseColor * u_materialFactor;
    out_Scale = vec4(Scale, 0.0);
    out_Normal = vec4(finalNormal, 0.0);
    out_Rotation = Quaternion;
    out_PBR = vec4(metallicRoughness, 1.0, 1.0);
}
)GLSL";

} // namespace shaders

//------------------------------------------------------------------------------
// ShaderManager Implementation
//------------------------------------------------------------------------------

class ShaderManager::Impl {
public:
    unsigned int converterProgram_ = 0;
    bool initialized_ = false;
    std::string errorMessage_;
    std::unordered_map<std::string, int> uniformLocations_;
    
    bool initialize() {
        // Compile vertex shader
        unsigned int vs = compileShader(GL_VERTEX_SHADER, shaders::converterVS);
        if (vs == 0) return false;
        
        // Compile geometry shader
        unsigned int gs = compileShader(GL_GEOMETRY_SHADER, shaders::converterGS);
        if (gs == 0) {
            glDeleteShader(vs);
            return false;
        }
        
        // Compile fragment shader
        unsigned int fs = compileShader(GL_FRAGMENT_SHADER, shaders::converterFS);
        if (fs == 0) {
            glDeleteShader(vs);
            glDeleteShader(gs);
            return false;
        }
        
        // Link program
        converterProgram_ = glCreateProgram();
        glAttachShader(converterProgram_, vs);
        glAttachShader(converterProgram_, gs);
        glAttachShader(converterProgram_, fs);
        glLinkProgram(converterProgram_);
        
        // Check link status
        int success;
        glGetProgramiv(converterProgram_, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetProgramInfoLog(converterProgram_, 1024, nullptr, infoLog);
            errorMessage_ = std::string("Shader program linking failed: ") + infoLog;
            glDeleteProgram(converterProgram_);
            converterProgram_ = 0;
        }
        
        // Cleanup shaders (they're linked into the program now)
        glDeleteShader(vs);
        glDeleteShader(gs);
        glDeleteShader(fs);
        
        if (converterProgram_ == 0) return false;
        
        initialized_ = true;
        return true;
    }
    
    unsigned int compileShader(GLenum type, const char* source) {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        
        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[1024];
            glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
            
            const char* typeName = "unknown";
            if (type == GL_VERTEX_SHADER) typeName = "vertex";
            else if (type == GL_GEOMETRY_SHADER) typeName = "geometry";
            else if (type == GL_FRAGMENT_SHADER) typeName = "fragment";
            
            std::ostringstream oss;
            oss << "Failed to compile " << typeName << " shader: " << infoLog;
            errorMessage_ = oss.str();
            
            glDeleteShader(shader);
            return 0;
        }
        
        return shader;
    }
    
    int getUniformLocation(const std::string& name) {
        auto it = uniformLocations_.find(name);
        if (it != uniformLocations_.end()) {
            return it->second;
        }
        int loc = glGetUniformLocation(converterProgram_, name.c_str());
        uniformLocations_[name] = loc;
        return loc;
    }
    
    void cleanup() {
        if (converterProgram_ != 0) {
            glDeleteProgram(converterProgram_);
            converterProgram_ = 0;
        }
        uniformLocations_.clear();
        initialized_ = false;
    }
};

ShaderManager::ShaderManager() : impl_(std::make_unique<Impl>()) {}
ShaderManager::~ShaderManager() { cleanup(); }

bool ShaderManager::initialize() {
    return impl_->initialize();
}

bool ShaderManager::isInitialized() const {
    return impl_->initialized_;
}

unsigned int ShaderManager::getConverterProgram() const {
    return impl_->converterProgram_;
}

void ShaderManager::useConverterProgram() {
    if (impl_->converterProgram_ != 0) {
        glUseProgram(impl_->converterProgram_);
    }
}

void ShaderManager::setUniform(const std::string& name, int value) {
    int loc = impl_->getUniformLocation(name);
    if (loc >= 0) glUniform1i(loc, value);
}

void ShaderManager::setUniform(const std::string& name, float value) {
    int loc = impl_->getUniformLocation(name);
    if (loc >= 0) glUniform1f(loc, value);
}

void ShaderManager::setUniform(const std::string& name, float x, float y) {
    int loc = impl_->getUniformLocation(name);
    if (loc >= 0) glUniform2f(loc, x, y);
}

void ShaderManager::setUniform(const std::string& name, float x, float y, float z) {
    int loc = impl_->getUniformLocation(name);
    if (loc >= 0) glUniform3f(loc, x, y, z);
}

void ShaderManager::setUniform(const std::string& name, float x, float y, float z, float w) {
    int loc = impl_->getUniformLocation(name);
    if (loc >= 0) glUniform4f(loc, x, y, z, w);
}

std::string ShaderManager::getErrorMessage() const {
    return impl_->errorMessage_;
}

void ShaderManager::cleanup() {
    impl_->cleanup();
}

} // namespace mesh2splat
