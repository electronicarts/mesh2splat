#version 450 core

#define PI 22.0f/7.0f

uniform sampler2D gPosition;
uniform sampler2D gAlbedo;
uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D gMetallicRoughness;

uniform mat4 u_worldToView;
uniform vec2 u_resolution;
uniform vec3 u_LightPosition;
uniform vec3 u_camPos;
uniform vec3 u_lightColor;
uniform bool u_isLightingEnalbed;
uniform float u_farPlane;
uniform float u_lightIntensity;
uniform int u_renderMode;
uniform samplerCube u_shadowCubemap;

in vec2 fragUV;
out vec4 FragColor;

//Very simple GGX pbr shader to show the material properties

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness * roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom       = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float computeShadowFactor(vec3 pos)
{
    vec3 sampleOffsetDirections[20] = vec3[]
    (
       vec3( 1,  1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1,  1,  1), 
       vec3( 1,  1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1,  1, -1),
       vec3( 1,  1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1,  1,  0),
       vec3( 1,  0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1,  0, -1),
       vec3( 0,  1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0,  1, -1)
    );


    vec3 lightDir       = pos - u_LightPosition;
    float currentDepth  = length(lightDir) ;
    vec3 sampleDir      = normalize(lightDir);

    float shadowFact    = 0;
    float bias          = 0.05;
    int samples         = 20;
    float diskRadius    = 0.025;

    //simple pcf
    for (int i = 0; i < samples; ++i)
    {
        float closestDepth  = texture(u_shadowCubemap, sampleDir + sampleOffsetDirections[i] * diskRadius).r * u_farPlane;
        shadowFact += currentDepth - bias > closestDepth ? 1.0 : 0.0;
    }
    return shadowFact / float(samples);

}

void main() {
    vec3 albedo         = texture(gAlbedo, fragUV).rgb;
    vec3 depth         = texture(gDepth, fragUV).rgb;

    if(u_renderMode == 5) //PBR props visualization
    {
        FragColor = vec4(texture(gMetallicRoughness, fragUV).rg, 0, 1);
        return;
    }
    if (!u_isLightingEnalbed) //change to "albedo", not clear otherwise
    {
        FragColor = vec4(albedo, 1.0f);
        return;
    }


    vec2 pbr            = texture(gMetallicRoughness, fragUV).xy; 
    float metallic      = pbr.x;
    float roughness     = pbr.y;

    vec3 pos            = texture(gPosition, fragUV).xyz;

    vec3 N              = normalize(texture(gNormal, fragUV).xyz * 2.0 - 1.0);

    float shadow        = computeShadowFactor(pos);

    albedo              = vec3(pow(albedo.x, 2.2f), pow(albedo.y, 2.2f), pow(albedo.z, 2.2f)); //linear to srgb, should probably just specify the albedo texture as srgband let sampler directly convert

    //Lighting
    vec3 L = normalize(u_LightPosition.xyz - pos);
    vec3 V = normalize(u_camPos - pos);

    vec3 H = normalize(V + L);

    float d = length(u_LightPosition.xyz - pos);
    float attenuation = 1.0 / (d * d);
    vec3 radiance = u_lightColor * u_lightIntensity * attenuation;

    vec3 F0 = vec3(0.04); 
    F0      = mix(F0, albedo, metallic);
    vec3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0); //ks
    
    float NDF = DistributionGGX(N, H, roughness);       
    float G   = GeometrySmith(N, V, L, roughness); 
    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001;
    vec3 specular     = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL * (1.0 - shadow);

    vec3 ambient = vec3(.3) * albedo;
    vec3 color = ambient + Lo;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 

    FragColor = vec4(color, 1.0);
}