#version 430 core
layout(vertices = 3) out;

in VS_OUT {
    vec3 position;
} tcs_in[];

out TCS_OUT {
    vec3 position;
} tcs_out[];

uniform float targetEdgeLength;

void main() {
    if (gl_InvocationID == 0) {
        // Calculate edge lengths
        float edge1 = length(tcs_in[1].position - tcs_in[0].position);
        float edge2 = length(tcs_in[2].position - tcs_in[1].position);
        float edge3 = length(tcs_in[0].position - tcs_in[2].position);

        gl_TessLevelInner[0] = max(edge1, max(edge2, edge3)) / targetEdgeLength;
        gl_TessLevelOuter[0] = edge1 / targetEdgeLength;
        gl_TessLevelOuter[1] = edge2 / targetEdgeLength;
        gl_TessLevelOuter[2] = edge3 / targetEdgeLength;
    }
    tcs_out[gl_InvocationID].position = tcs_in[gl_InvocationID].position;
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
