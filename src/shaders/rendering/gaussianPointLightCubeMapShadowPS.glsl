///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#version 460 core

in vec3 out_pos;

uniform float u_near; 
uniform float u_far;  

uniform vec3 u_lightPos;
uniform float u_farPlane;

void main() {
    gl_FragDepth = length(out_pos - u_lightPos) / u_farPlane;
}