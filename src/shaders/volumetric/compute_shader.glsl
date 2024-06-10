#version 460 core

layout(local_size_x = 16, local_size_y = 16) in;

layout(rgba32f, binding = 0) uniform image2D mainPositionMap;
layout(rgba32f, binding = 1) uniform image2D mainNormalMap;
layout(rgba32f, binding = 2) uniform image2D mainQuaternionMap;
layout(rgba32f, binding = 3) uniform image2D mainColorMap;
layout(rgba32f, binding = 4) uniform image2D mainMetallicRoughnessMap;

layout(rgba32f, binding = 5) uniform image2D micromeshPositionMap;
layout(rgba32f, binding = 6) uniform image2D micromeshNormalMap;
layout(rgba32f, binding = 7) uniform image2D micromeshQuaternionMap;
layout(rgba32f, binding = 8) uniform image2D micromeshColorMap;
layout(rgba32f, binding = 9) uniform image2D micromeshMetallicRoughnessMap;

layout(std430, binding = 10) buffer OutputBuffer {
    vec4 outputPositions[];
    vec4 outputNormals[];
    vec4 outputQuaternions[];
    vec4 outputColors[];
    vec4 outputMetallicRoughness[];
};

uniform sampler2D distributionMap;
uniform int numMicromeshInstances;
uniform vec3 meshScale;

void main() {
    uint index = gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * gl_NumWorkGroups.x;

    if (index >= numMicromeshInstances) return;

    ivec2 texSize = textureSize(distributionMap, 0);
    ivec2 texCoord = ivec2(gl_GlobalInvocationID.xy % texSize);
    vec4 texValue = texelFetch(distributionMap, texCoord, 0);

    // Calculate where to place the micromesh on the main mesh's surface
    ivec2 mainMeshCoord = ivec2(texValue.r * texSize.x, texValue.g * texSize.y); // Adjust this logic as needed

    vec4 mainPosition = imageLoad(mainPositionMap, mainMeshCoord);
    vec4 mainNormal = imageLoad(mainNormalMap, mainMeshCoord);
    vec4 mainQuaternion = imageLoad(mainQuaternionMap, mainMeshCoord);
    vec4 mainColor = imageLoad(mainColorMap, mainMeshCoord);
    vec4 mainMetallicRoughness = imageLoad(mainMetallicRoughnessMap, mainMeshCoord);

    // Place micromesh instances based on main mesh data
    for (int i = 0; i < numMicromeshInstances; ++i) {
        ivec2 micromeshCoord = ivec2(float(i) / float(numMicromeshInstances) * texSize.x, texCoord.y);

        vec4 micromeshPosition = imageLoad(micromeshPositionMap, micromeshCoord);
        vec4 micromeshNormal = imageLoad(micromeshNormalMap, micromeshCoord);
        vec4 micromeshQuaternion = imageLoad(micromeshQuaternionMap, micromeshCoord);
        vec4 micromeshColor = imageLoad(micromeshColorMap, micromeshCoord);
        vec4 micromeshMetallicRoughness = imageLoad(micromeshMetallicRoughnessMap, micromeshCoord);

        // Transform micromesh positions by main mesh position and scale
        vec3 worldPosition = mainPosition.xyz + micromeshPosition.xyz * meshScale;
        outputPositions[index * numMicromeshInstances + i] = vec4(worldPosition, 1.0);

        // Combine normals and orientations
        vec3 worldNormal = normalize(mainNormal.xyz + micromeshNormal.xyz);
        outputNormals[index * numMicromeshInstances + i] = vec4(worldNormal, 0.0);

        vec4 combinedQuaternion = mix(mainQuaternion, micromeshQuaternion, 0.5); // Simple blend
        outputQuaternions[index * numMicromeshInstances + i] = combinedQuaternion;

        // Combine colors and material properties
        vec4 combinedColor = mix(mainColor, micromeshColor, 0.5);
        outputColors[index * numMicromeshInstances + i] = combinedColor;

        vec4 combinedMetallicRoughness = mix(mainMetallicRoughness, micromeshMetallicRoughness, 0.5);
        outputMetallicRoughness[index * numMicromeshInstances + i] = combinedMetallicRoughness;
    }
}
