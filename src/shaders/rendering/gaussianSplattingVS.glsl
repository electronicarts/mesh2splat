///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#version 460 core

// Static quad vertex position per-vertex
layout(location = 0) in vec4 vertexPos;

// Per-instance (quad) attributes
layout(location = 1) in vec4 gaussianMean2Ndc; 
layout(location = 2) in vec4 quadScaleNdc;
layout(location = 3) in vec4 color;
layout(location = 4) in vec4 conic;
layout(location = 5) in vec4 normal;
layout(location = 6) in vec4 gaussianWsPos;


uniform vec2 u_resolution;

out vec3 out_color;
out vec2 out_screen;
out float out_opacity;
out vec3 out_conic;
out vec3 out_normal;
out vec3 out_wsPos;
out float out_depth;
out vec2 metallicRoughness;

void main() {
	gl_Position = vec4(gaussianMean2Ndc.xy + (vertexPos.x * quadScaleNdc.xy + vertexPos.y * quadScaleNdc.zw) , 0, 1);
    out_conic = vec3(-0.5 * conic.x,  -conic.y, -0.5 * conic.z);
    out_color = color.rgb * color.a;
	out_opacity = color.a;
	out_screen = vec2((gaussianMean2Ndc.xy + 1) * 0.5 * u_resolution);
	out_normal = normal.xyz;
	out_wsPos = gaussianWsPos.xyz;
	out_depth = conic.w; //I just added the depth value here, easier
	metallicRoughness = vec2(normal.w, gaussianWsPos.w);
}
