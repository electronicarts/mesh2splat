#version 460 core

layout(binding = 0) uniform sampler2D texPositionAndScaleX;  
layout(binding = 1) uniform sampler2D scaleZAndNormal;
layout(binding = 2) uniform sampler2D rotationAsQuat;
layout(binding = 3) uniform sampler2D texColor;
layout(binding = 4) uniform sampler2D pbrAndScaleY;


struct GaussianVertex {
    vec4 position;
    vec4 color;
    vec4 scale;
    vec4 rotation;
    vec4 pbr;
};

layout(std430, binding = 5) buffer GaussianBuffer {
    GaussianVertex vertices[];
} gaussianBuffer;

layout(std430, binding = 6) buffer DrawCommand {
    uint count;
    uint instanceCount;
    uint first;
    uint baseInstance;
} drawElementsIndirectCommand;


layout(local_size_x = 16, local_size_y = 16) in;  
void main() {
    if (gl_GlobalInvocationID.x == 0 && gl_GlobalInvocationID.y == 0) {
        drawElementsIndirectCommand.count = 6; //for quad rendering        
        drawElementsIndirectCommand.instanceCount = 0;    
        drawElementsIndirectCommand.first = 0;        
        drawElementsIndirectCommand.baseInstance = 0; 
    }
    memoryBarrierShared();

    ivec2 pix       = ivec2(gl_GlobalInvocationID.xy);

    vec4 posAndScaleXData       = texelFetch(texPositionAndScaleX, pix, 0);

    if (posAndScaleXData == vec4(0, 0, 0, 0)) return; //TOOD: naive check

    uint index      = atomicAdd(drawElementsIndirectCommand.instanceCount, 1);

    //should change ordering and leave scale as first components, otherwise confusing
    //vec4 posAndScaleXData       = texelFetch(texPositionAndScaleX, pix, 0);
    vec4 position               = vec4(posAndScaleXData.xyz, 1);

    vec4 scaleZAndNormalData    = texelFetch(scaleZAndNormal, pix, 0);
    vec4 normal                 = vec4(scaleZAndNormalData.yzw, 0);

    vec4 quaternion             = texelFetch(rotationAsQuat, pix, 0);

    vec4 colorData              = texelFetch(texColor, pix, 0);
    vec4 pbrAndScaleY           = texelFetch(pbrAndScaleY, pix, 0);

    vec4 scale                  = vec4(posAndScaleXData.w, pbrAndScaleY.z, scaleZAndNormalData.x, 1.0);

    gaussianBuffer.vertices[index].position     = position;
    gaussianBuffer.vertices[index].color        = colorData; 
    gaussianBuffer.vertices[index].scale        = scale; 
    gaussianBuffer.vertices[index].rotation     = quaternion; 
    gaussianBuffer.vertices[index].pbr          = vec4(pbrAndScaleY.xy, 0 ,0); 
}
