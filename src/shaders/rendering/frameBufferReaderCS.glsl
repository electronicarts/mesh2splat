#version 460 core

//TODO: change naming to u_
layout(binding = 0) uniform sampler2D texPositionAndScaleX;  
layout(binding = 1) uniform sampler2D scaleZAndNormal;
layout(binding = 2) uniform sampler2D rotationAsQuat;
layout(binding = 3) uniform sampler2D texColor;
layout(binding = 4) uniform sampler2D pbrAndScaleY;


struct GaussianVertex {
    vec4 position;
    vec4 color;
    vec4 scale;
    vec4 normal;
    vec4 rotation;
    vec4 pbr;
};

layout(std430, binding = 5) buffer GaussianBuffer {
    GaussianVertex vertices[];
} gaussianBuffer;

layout(binding = 6) uniform atomic_uint g_validCounter;


layout(local_size_x = 16, local_size_y = 16) in;  
void main() {

    ivec2 pix       = ivec2(gl_GlobalInvocationID.xy);

    vec4 posAndScaleXData       = texelFetch(texPositionAndScaleX, pix, 0);

    if (posAndScaleXData == vec4(0, 0, 0, 0)  ) return; //TOOD: naive check, find better way

    uint gaussianIndex = atomicCounterIncrement(g_validCounter);

    //should change ordering and leave scale as first components, otherwise confusing
    vec4 position               = vec4(posAndScaleXData.xyz, 1);

    vec4 scaleZAndNormalData    = texelFetch(scaleZAndNormal, pix, 0);
    vec4 normal                 = vec4(scaleZAndNormalData.yzw, 0);

    vec4 quaternion             = texelFetch(rotationAsQuat, pix, 0);

    vec4 colorData              = texelFetch(texColor, pix, 0);
    vec4 pbrAndScaleY           = texelFetch(pbrAndScaleY, pix, 0);

    vec4 scale                  = vec4(posAndScaleXData.w, pbrAndScaleY.z, scaleZAndNormalData.x, 1.0);

    //if (any(isinf(scale)) || any(isnan(scale)) || any(scale == 0) || any(greaterThan(vec4_value, vec4(10.0))) || any(lessThan(vec4_value, vec4(-6)))) return;

    //TODO: I could save at least one vec4 by packing the pbr properties i the .w of position and .w of color
    //gaussianBuffer.vertices[gaussianIndex].position     = position;
    //gaussianBuffer.vertices[gaussianIndex].color        = colorData; 
    //gaussianBuffer.vertices[gaussianIndex].scale        = scale; 
    //gaussianBuffer.vertices[gaussianIndex].normal       = normal; 
    //gaussianBuffer.vertices[gaussianIndex].rotation     = quaternion; 
    //gaussianBuffer.vertices[gaussianIndex].pbr          = vec4(pbrAndScaleY.xy, 0 , 1); //last one is a flag to check wheter it was read from mesh or standard ply (1: mesh)
}
