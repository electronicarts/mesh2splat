///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#version 460 core

layout(location = 0) out vec4 gPosition; 
layout(location = 1) out vec4 gNormal;   
layout(location = 2) out vec4 gAlbedo;  
layout(location = 3) out vec4 gDepth;   
layout(location = 4) out vec4 gMetallicRoughness;   


in vec3 out_color;
in float out_opacity;
in vec2 out_screen;
in vec3 out_conic;
in vec3 out_normal;
in vec3 out_wsPos;
in float out_depth;
in vec2 metallicRoughness;

uniform vec2 u_resolution;
uniform int u_renderMode;

out vec4 FragColor;

void main() {	
    vec2 d = (out_screen - gl_FragCoord.xy);
    float alpha = dot(out_conic.xzy, vec3(d*d, d.x*d.y));
    float g = exp(alpha);

    if (u_renderMode == 4)
        gAlbedo = vec4(.01, .005, 0, .01);
    else
        gAlbedo = vec4(out_color, out_opacity) * g;

    gPosition           = vec4(out_wsPos, 1.0) * g;
    gNormal             = vec4(out_normal, out_opacity) * g;
    
    float depthPrem     = out_depth * g;
    gDepth              = vec4(depthPrem, depthPrem, depthPrem, out_opacity * g);
    
    gMetallicRoughness  = vec4(metallicRoughness, 0, 1.0) * g;
}