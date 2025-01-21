#version 460 core

// Static quad vertex position per-vertex
layout(location = 0) in vec3 vertexPos;

// Per-instance attributes
layout(location = 1) in vec4 instanceOffset;  
layout(location = 2) in vec4 instanceColor;
layout(location = 3) in vec4 a;
layout(location = 4) in vec4 b;
layout(location = 5) in vec4 c;

// You can add more per-instance attributes at locations 3, 4, 5 as needed.
out vec3 out_color;

uniform mat4 MVP;

void main() {
    // Combine static vertex position with per-instance offset
    vec4 worldPosition = vec4(vertexPos.xyz * 0.1f + instanceOffset.xyz, 1.0);
    
    // Transform the vertex position to clip space
    gl_Position = MVP * worldPosition;
    
    // Pass instance color to fragment shader
    out_color = instanceColor.rgb;
}
