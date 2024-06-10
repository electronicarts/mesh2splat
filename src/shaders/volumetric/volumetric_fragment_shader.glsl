#version 460 core

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

uniform vec3 meshMaterialColor;
uniform int hasAlbedoMap;
uniform int hasNormalMap;
uniform int hasMetallicRoughnessMap;

// Inputs from the geometry shader
in vec3 GaussianPosition;
in vec3 Scale;
in vec2 UV;
in vec4 Tangent;
in vec3 Normal;
in vec4 Quaternion;

void main() {
	//vec2 MetallicRoughness = vec2(0.2f, 0.5f); //Set these defaults from uniforms
	// Pack Gaussian parameters into the output fragments
	FragColor0 = vec4(GaussianPosition.x, GaussianPosition.y, GaussianPosition.z, Scale.x);
	FragColor1 = vec4(Scale.z, Normal.x, Normal.y, Normal.z);
	FragColor2 = vec4(Quaternion.x, Quaternion.y, Quaternion.z, Quaternion.w);
	FragColor3 = vec4(meshMaterialColor, 1.0f);
	FragColor4 = vec4(0.05f, 0.5f, 0.0f, 0.0f);
}
