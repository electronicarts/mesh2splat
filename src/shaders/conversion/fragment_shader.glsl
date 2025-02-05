#version 460 core

//change layout and use different outs
uniform sampler2D albedoTexture;
uniform sampler2D normalTexture;
uniform sampler2D metallicRoughnessTexture;
uniform sampler2D occlusionTexture;
uniform sampler2D emissiveTexture;

uniform int hasAlbedoMap;
uniform int hasNormalMap;
uniform int hasMetallicRoughnessMap;

struct GaussianVertex {
    vec4 position;
    vec4 color;
    vec4 scale;
    vec4 normal;
    vec4 rotation;
    vec4 pbr;
};

layout(std430, binding = 0) buffer GaussianBuffer {
    GaussianVertex vertices[];
} gaussianBuffer;

layout(binding = 1) uniform atomic_uint g_validCounter;

// Inputs from the geometry shader
in vec3 V1;
in vec3 V2;
in vec3 V3;
in vec3 Barycentric;
in vec3 Scale;
in vec2 UV;
in vec4 Tangent;
in vec3 Normal;
in vec4 Quaternion;

void main() {
    
    uint index = atomicCounterIncrement(g_validCounter);

    vec4 out_Color = vec4(0,0,0,1);

    if (hasAlbedoMap == 1)
    {
        out_Color = texture(albedoTexture, UV);
    }

    //NORMAL MAP
    //Should compute the TBN in geometry shader
    vec3 out_Normal;
    if (hasNormalMap == 1)
    {
        vec3 normalMap_normal = texture(normalTexture, UV).xyz;
        vec3 retrievedNormal = normalize(normalMap_normal.xyz * 2.0f - 1.0f); 

        vec3 bitangent = normalize(cross(Normal, Tangent.xyz)) * Tangent.w; //tangent.w is the bitangent sign
        mat3 TBN = mat3(Tangent.xyz, bitangent, normalize(Normal));

        out_Normal = normalize(TBN * retrievedNormal);
    }
    else {
        out_Normal = Normal;
    }

    //METALLIC-ROUGHNESS MAP
    vec2 MetallicRoughness;
    if (hasMetallicRoughnessMap == 1)
    {
        vec2 metalRough = texture(metallicRoughnessTexture, UV).bg; //Blue contains metallic and Green roughness
        MetallicRoughness = vec2(metalRough.x, metalRough.y);
    }
    else {
        MetallicRoughness = vec2(0.1f, 0.5f); //Set these defaults from uniforms
    }

    vec3 pos = V1 * Barycentric.x + V2 * Barycentric.y + V3 * Barycentric.z;
    // Pack Gaussian parameters into the output fragments

    gaussianBuffer.vertices[index].position = vec4(pos, 1);
    gaussianBuffer.vertices[index].color = vec4(out_Color.rgb, 1);
    gaussianBuffer.vertices[index].scale = vec4(Scale, 0.0);
    gaussianBuffer.vertices[index].normal = vec4(out_Normal, 0.0);
    gaussianBuffer.vertices[index].rotation = Quaternion;
    gaussianBuffer.vertices[index].pbr = vec4(MetallicRoughness, 0, 1);
}
