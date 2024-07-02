#version 460 core

struct Gaussian3D {
    float GaussianPosition[3];
    float Scale[3];
    float UV[2];
    float Normal[3];
    float Quaternion[4];
    float Rgba[4];
};

layout(std430, binding = 0) coherent buffer OutputBuffer {
    Gaussian3D FragData[];
};

layout(binding = 1) uniform atomic_uint counter; //Use it to visualize color for debugging

// Inputs from the geometry shader
in vec3 GaussianPosition;
in vec3 Scale;
in vec2 UV;
in vec4 Tangent;
in vec3 Normal;
in vec4 Quaternion;

void main() {
    // Atomically increment the counter to get a unique index
    uint index = atomicCounterIncrement(counter);

    // Pack Gaussian parameters into the output fragments
    FragData[index].GaussianPosition[0] = GaussianPosition.x;
    FragData[index].GaussianPosition[1] = GaussianPosition.y;
    FragData[index].GaussianPosition[2] = GaussianPosition.z;

    FragData[index].Scale[0] = Scale.x;
    FragData[index].Scale[1] = Scale.y;
    FragData[index].Scale[2] = Scale.z;

    FragData[index].UV[0] = UV.x;
    FragData[index].UV[1] = UV.y;

    FragData[index].Normal[0] = Normal.x;
    FragData[index].Normal[1] = Normal.y;
    FragData[index].Normal[2] = Normal.z;

    FragData[index].Quaternion[0] = Quaternion.x;
    FragData[index].Quaternion[1] = Quaternion.y;
    FragData[index].Quaternion[2] = Quaternion.z;
    FragData[index].Quaternion[3] = Quaternion.w;

    FragData[index].Rgba[0] = 0.2f;
    FragData[index].Rgba[1] = 0.5f;
    FragData[index].Rgba[2] = 0.7f;
    FragData[index].Rgba[3] = 1.0f;
}
