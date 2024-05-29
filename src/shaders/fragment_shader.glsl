#version 430 core

//change layout and use different outs
layout(location = 0) out vec4 FragColor0;
layout(location = 1) out vec4 FragColor1;
layout(location = 2) out vec4 FragColor2;
layout(location = 3) out vec4 FragColor3;
layout(location = 4) out vec4 FragColor4;

uniform sampler2D albedoTexture;
uniform sampler2D normalTexture;
uniform sampler2D metallicRoughnessTexture;
uniform sampler2D occlusionTexture;
uniform sampler2D emissiveTexture;

// Inputs from the geometry shader
in vec3 GaussianPosition;
in vec3 Scale;
in vec2 UV;
in vec4 Tangent;
in vec3 Normal;
in vec4 Quaternion;

void main() {
    //Do ablation with this:
    //vec3 u = dFdx(GaussianPosition);
    //vec3 v = dFdy(GaussianPosition);
    //vec3 n = normalize(cross(u, v));
    
    //NORMAL MAP
    //Should compute this in geometry shader
    vec3 normalMap_normal = texture(normalTexture, UV).xyz;
    //https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#_material_normaltextureinfo_scale
    vec3 retrievedNormal = normalize(normalMap_normal.xyz * 2.0f - 1.0f); //TODO: * vec3(material.normalScale, material.normalScale, 1.0f)); 
    
    vec3 bitangent = normalize(cross(Normal, Tangent.xyz)) * Tangent.w; //tangent.w is the bitangent sign
    mat3 TBN = mat3(Tangent.xyz, bitangent, Normal);

    vec3 out_Normal = TBN * retrievedNormal;

    //METALLIC-ROUGHNESS MAP
    vec2 metalRough = texture(metallicRoughnessTexture, UV).bg; //Blue contains metallic and Green roughness
    vec2 MetallicRoughness = vec2(metalRough.x, metalRough.y);

    // Pack Gaussian parameters into the output fragments
    FragColor0 = vec4(GaussianPosition.x, GaussianPosition.y, GaussianPosition.z, Scale.x);
    FragColor1 = vec4(Scale.z, out_Normal.x, out_Normal.y, out_Normal.z);
    FragColor2 = vec4(Quaternion.x, Quaternion.y, Quaternion.z, Quaternion.w); 
    FragColor3 = texture(albedoTexture, UV);
    FragColor4 = vec4(MetallicRoughness, 0.0f, 0.0f);
}
