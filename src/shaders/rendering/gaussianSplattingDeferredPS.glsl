#version 450 core

#define PI 22.0f/7.0f

uniform sampler2D gPosition;
uniform sampler2D gAlbedo;
uniform sampler2D gDepth;
uniform sampler2D gNormal;
uniform sampler2D gMetallicRoughness;

uniform mat4 u_clipToView;
uniform mat4 u_worldToView;
uniform vec2 u_resolution;
uniform vec3 u_LightPosition;
uniform vec3 u_camPos;
uniform bool u_isLightingEnalbed;

in vec2 fragUV;
out vec4 FragColor;

vec3 getViewPosition(vec2 uv, float depth) {
    vec3 ndc = vec3(uv * 2.0 - 1.0, depth);
    vec4 viewPos = u_clipToView * vec4(ndc, 1.0);
    return viewPos.xyz / viewPos.w;
}

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

void main() {
    vec3 albedo         = texture(gAlbedo, fragUV).rgb;
    vec3 depth         = texture(gDepth, fragUV).rgb;


    if (!u_isLightingEnalbed)
    {
        FragColor = vec4(albedo, 1.0f);
        return;
    }

    vec2 pbr            = texture(gMetallicRoughness, fragUV).xy; 
    float metallic      = pbr.x;
    float roughness     = pbr.y;

    vec3 pos            = texture(gPosition, fragUV).xyz;

    vec3 N              = normalize(texture(gNormal, fragUV).xyz * 2.0 - 1.0);

    vec3 dx = dFdxFine(pos);
    vec3 dy = dFdyFine(pos);
    float magDx = length(dx);
    float magDy = length(dy);

    // define a threshold for "unreasonably large"
    float threshold = 10.0; // depends on your scene scale

    if (magDx > threshold || magDy > threshold) {
        // fallback to something simpler (e.g. old normal or a default)
        dx = vec3(0, 0, 0);
        dy = vec3(0, 0, 1);
    }

    vec3 n = normalize(cross(dx, dy));

    albedo              = vec3(pow(albedo.x, 2.2f), pow(albedo.y, 2.2f), pow(albedo.z, 2.2f)); //linear to srgb, should probably just specify the albedo texture as srgband let sampler directly convert

    //Lighting
    vec3 L = normalize(u_LightPosition.xyz - pos);
    vec3 V = normalize(u_camPos - pos);

    //if(dot(V, N)  0) N*=-1;

    vec3 H = normalize(V + L);

    float d = length(u_LightPosition.xyz - pos);
    float attenuation = 1.0 / (d * d);
    float lightIntensity = 140.0;
    vec3 radiance = vec3(1, 1, 1) * lightIntensity * attenuation;

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
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = vec3(.3) * albedo;
    vec3 color = ambient + Lo;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2)); 


    FragColor = vec4(color, 1.0);
}