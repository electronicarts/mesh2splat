#version 460 core
layout(vertices = 3) out;

uniform int tesselationFactorMultiplier;

in VS_OUT {
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
    vec2 normalizedUv;
    vec3 scale;
} tcs_in[];

out TCS_OUT {
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
    vec2 normalizedUv;
    vec3 scale;
} tcs_out[];

void main() { 

    float edge1 = length(tcs_in[0].position - tcs_in[1].position);
    float edge2 = length(tcs_in[1].position - tcs_in[2].position);
    float edge3 = length(tcs_in[2].position - tcs_in[0].position);
    //loat perimeter = edge1 + edge2 + edge3; //No need

    //float sizeFactor = clamp(perimeter / medianPerimeter, 0.5, 2.0);

    int tessFactor1 = int(max(1.0, edge1 * tesselationFactorMultiplier));
    int tessFactor2 = int(max(1.0, edge2 * tesselationFactorMultiplier));
    int tessFactor3 = int(max(1.0, edge3 * tesselationFactorMultiplier));

    gl_TessLevelOuter[0] = tessFactor1;
    gl_TessLevelOuter[1] = tessFactor2;
    gl_TessLevelOuter[2] = tessFactor3;
                           
    gl_TessLevelInner[0] = (tessFactor1 + tessFactor2 + tessFactor3) / 3;

    tcs_out[gl_InvocationID].position = tcs_in[gl_InvocationID].position;
    tcs_out[gl_InvocationID].normal = tcs_in[gl_InvocationID].normal;
    tcs_out[gl_InvocationID].tangent = tcs_in[gl_InvocationID].tangent;
    tcs_out[gl_InvocationID].uv = tcs_in[gl_InvocationID].uv;
    tcs_out[gl_InvocationID].normalizedUv = tcs_in[gl_InvocationID].normalizedUv;
    tcs_out[gl_InvocationID].scale = tcs_in[gl_InvocationID].scale;
}
