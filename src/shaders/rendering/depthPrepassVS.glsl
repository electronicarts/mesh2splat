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

uniform mat4 u_worldToView;
uniform mat4 u_viewToClip;
uniform mat4 u_modelToWorld;

void main() {
    gl_Position     = (u_viewToClip * u_worldToView * u_modelToWorld * vec4(position, 1.0));
}
