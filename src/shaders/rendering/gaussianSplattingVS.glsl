#version 460 core

layout(location = 0) in vec4 position;
layout(location = 1) in vec4 color;

out vec3 out_color;

uniform mat4 MVP; 
uniform float std_gauss;

void main() {
    gl_Position = MVP * vec4(position.xyz, 1.0);
    gl_PointSize = std_gauss; 
    out_color = color.rgb;
}