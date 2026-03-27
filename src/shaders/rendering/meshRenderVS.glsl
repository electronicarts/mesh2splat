///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#version 460 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec2 normalizedUv;
layout(location = 5) in vec3 scale;

uniform mat4 u_modelToWorld;
uniform mat4 u_worldToView;
uniform mat4 u_viewToClip;

out vec3 v_worldPos;
out vec3 v_normal;
out vec4 v_tangent;
out vec2 v_uv;
out float v_viewDepth;

void main() {
    vec4 worldPos = u_modelToWorld * vec4(position, 1.0);
    v_worldPos = worldPos.xyz;

    mat3 normalMatrix = mat3(transpose(inverse(u_modelToWorld)));
    v_normal = normalize(normalMatrix * normal);
    v_tangent = vec4(normalize(normalMatrix * tangent.xyz), tangent.w);
    v_uv = uv;

    vec4 viewPos = u_worldToView * worldPos;
    v_viewDepth = -viewPos.z; // positive depth in view space

    gl_Position = u_viewToClip * viewPos;
}
