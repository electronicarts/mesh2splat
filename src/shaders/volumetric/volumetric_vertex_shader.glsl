#version 460 core

// Input attributes
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec2 normalizedUv;
layout(location = 5) in vec3 scale;

uniform mat4 modelMatrix;

// Output struct definition
out VS_OUT{
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
    vec2 normalizedUv;
    vec3 scale;
} vs_out;

void main() {
    vs_out.position = vec3(modelMatrix * vec4(position, 1.0));
    vs_out.normal = normalize(vec3(mat3(modelMatrix) * normal));
    vs_out.position += normalize(vec3(modelMatrix[2])) * 0.02f;
    vs_out.tangent = normalize(vec4(mat3(modelMatrix) * tangent.xyz, tangent.w));
    vs_out.uv = uv;
    vs_out.normalizedUv = normalizedUv;
    vs_out.scale = scale;
}
