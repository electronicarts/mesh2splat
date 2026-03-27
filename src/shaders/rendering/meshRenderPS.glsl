///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#version 460 core

// G-buffer outputs (same layout as splat G-buffer)
layout(location = 0) out vec4 gPosition;
layout(location = 1) out vec4 gNormal;
layout(location = 2) out vec4 gAlbedo;
layout(location = 3) out vec4 gDepth;
layout(location = 4) out vec4 gMetallicRoughness;

in vec3 v_worldPos;
in vec3 v_normal;
in vec4 v_tangent;
in vec2 v_uv;
in float v_viewDepth;

uniform sampler2D albedoTexture;
uniform sampler2D normalTexture;
uniform sampler2D metallicRoughnessTexture;

uniform int hasAlbedoMap;
uniform int hasNormalMap;
uniform int hasMetallicRoughnessMap;
uniform vec4 u_baseColorFactor;
uniform vec2 u_nearFar;
uniform int u_renderMode;

#include "common.glsl"

void main() {
    // Albedo
    vec4 albedo = u_baseColorFactor;
    if (hasAlbedoMap == 1) {
        albedo *= texture(albedoTexture, v_uv);
    }

    // Normal (with optional normal mapping)
    vec3 N = normalize(v_normal);
    if (hasNormalMap == 1) {
        vec3 normalMapValue = texture(normalTexture, v_uv).xyz;
        vec3 mappedNormal = normalize(normalMapValue * 2.0 - 1.0);

        vec3 T = normalize(v_tangent.xyz);
        vec3 B = normalize(cross(N, T)) * v_tangent.w;
        mat3 TBN = mat3(T, B, N);

        N = normalize(TBN * mappedNormal);
    }

    // Encode normal to [0,1] range (matching splat convention)
    vec3 encodedNormal = encodeNormal(N);

    // Metallic-roughness
    vec2 metalRough = vec2(0.1, 0.5); // defaults
    if (hasMetallicRoughnessMap == 1) {
        metalRough = texture(metallicRoughnessTexture, v_uv).bg; // b=metallic, g=roughness
    }

    // Depth visualization (matching splat prepass convention)
    float computedDepth = computeExponentialDepth(v_viewDepth, u_nearFar);

    // Per-triangle random color for geometry visualization (matches splat per-gaussian random)
    float triHash = fract(sin(float(gl_PrimitiveID) * 127.1) * 43758.5453);
    float triR = fract(sin(float(gl_PrimitiveID) * 311.7) * 43758.5453);
    float triG = fract(sin(float(gl_PrimitiveID) * 269.5 + 1.3) * 43758.5453);
    float triB = fract(sin(float(gl_PrimitiveID) * 183.3 + 2.7) * 43758.5453);

    // Handle render modes (same as splat prepass)
    vec4 outputColor;
    if (u_renderMode == 0 || u_renderMode == 6) {
        outputColor = albedo;                                       // Albedo / Final
    } else if (u_renderMode == 1) {
        outputColor = vec4(vec3(computedDepth), 1.0);               // Depth
    } else if (u_renderMode == 2) {
        outputColor = vec4(encodedNormal, 1.0);                     // Normal
    } else if (u_renderMode == 3) {
        outputColor = vec4(triR, triG, triB, 1.0);                  // Geometry (per-triangle random color)
    } else if (u_renderMode == 4) {
        outputColor = vec4(0.01, 0.005, 0.0, 0.01);                // Overdraw
    } else {
        outputColor = albedo;                                       // PBR fallback to albedo
    }

    // Write G-buffer (no premultiplication needed; mesh is opaque, no alpha blending)
    gPosition           = vec4(v_worldPos, 1.0);
    gNormal             = vec4(encodedNormal, 1.0);
    gAlbedo             = vec4(outputColor.rgb, 1.0);
    gDepth              = vec4(vec3(computedDepth), 1.0);
    gMetallicRoughness  = vec4(metalRough.x, metalRough.y, 0.0, 1.0);
}
