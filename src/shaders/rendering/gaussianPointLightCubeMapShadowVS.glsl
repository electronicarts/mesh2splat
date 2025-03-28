///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#version 460 core

layout(location = 0) in vec4 vertexPos;

//per instance
layout(location = 1) in vec4 gaussianMean2LightNdc; 
layout(location = 2) in vec4 quadScaleLightNdc;
layout(location = 3) in vec4 position;

uniform vec2 u_shadowResolution;

out vec3 out_pos;

void main() {
    out_pos = position.xyz; 
    gl_Position = vec4(gaussianMean2LightNdc.xy + (vertexPos.x * quadScaleLightNdc.xy + vertexPos.y * quadScaleLightNdc.zw), 0, 1);
}