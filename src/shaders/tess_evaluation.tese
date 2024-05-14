#version 430 core
layout(triangles, equal_spacing, cw) in;

in TCS_OUT {
    vec3 position;
} tes_in[];

out vec3 outPosition;

void main() {
    vec3 p0 = gl_TessCoord.x * tes_in[0].position;
    vec3 p1 = gl_TessCoord.y * tes_in[1].position;
    vec3 p2 = gl_TessCoord.z * tes_in[2].position;
    outPosition = p0 + p1 + p2; 
    gl_Position = vec4(outPosition, 1.0);
}
