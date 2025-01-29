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

layout(std430, binding = 6) writeonly buffer DrawCommand {
    uint count;
    uint instanceCount;
    uint first;
    uint baseVertex;
    uint baseInstance;
} drawElementsIndirectCommand;


layout(local_size_x = 16, local_size_y = 16) in;  
void main() {

    ivec2 pix       = ivec2(gl_GlobalInvocationID.xy);

    vec4 posAndScaleXData       = texelFetch(texPositionAndScaleX, pix, 0);

    if (posAndScaleXData == vec4(0, 0, 0, 0)) return; //TOOD: naive check, find better way

    uint index      = atomicAdd(drawElementsIndirectCommand.instanceCount, 1);

    //should change ordering and leave scale as first components, otherwise confusing
    vec4 position               = vec4(posAndScaleXData.xyz, 1);

    vec4 scaleZAndNormalData    = texelFetch(scaleZAndNormal, pix, 0);
    vec4 normal                 = vec4(scaleZAndNormalData.yzw, 0);

    vec4 quaternion             = texelFetch(rotationAsQuat, pix, 0);

    vec4 colorData              = texelFetch(texColor, pix, 0);
    vec4 pbrAndScaleY           = texelFetch(pbrAndScaleY, pix, 0);

    vec4 scale                  = vec4(posAndScaleXData.w, pbrAndScaleY.z, scaleZAndNormalData.x, 1.0);

    //TODO: I could save at least one vec4 by packing the pbr properties in the .w of position and .w of color
    gaussianBuffer.vertices[index].position     = position;
    gaussianBuffer.vertices[index].color        = colorData; 
    gaussianBuffer.vertices[index].scale        = scale; 
    gaussianBuffer.vertices[index].normal       = normal; 
    gaussianBuffer.vertices[index].rotation     = quaternion; 
    gaussianBuffer.vertices[index].pbr          = vec4(pbrAndScaleY.xy, 0 , 1); //last one is a flag to check wheter it was read from mesh or standard ply (1: mesh)
}
