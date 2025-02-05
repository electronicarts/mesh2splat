#version 460

struct QuadNdcTransformation {
    vec4 gaussianMean2dNdc;
    vec4 quadScaleNdc;
    vec4 color;
    vec4 conic;
};


layout(std430, binding = 0) buffer GaussiansIn  { vec4 in_data[];  };
layout(std430, binding = 1) buffer GaussiansOut { vec4 out_data[]; };
layout(std430, binding = 2) buffer Values       { uint val[]; };

layout(std430, binding = 3) writeonly buffer DrawCommand {
    uint count;
    uint instanceCount;
    uint first;
    uint baseInstance;
} drawElementsIndirectCommand;

uniform uint u_count;

layout(local_size_x = 16, local_size_y = 16) in;
void main() {
    uint globalWidth = gl_NumWorkGroups.x * gl_WorkGroupSize.x;
    uint gid = gl_GlobalInvocationID.y * globalWidth + gl_GlobalInvocationID.x;

    if (gid >= u_count) return;

    uint old_index = val[gid]; 
    //I am basically doing a buffer swap based on the values (indices) computed during the radxi sort pass
    uint vec4ByteStride = 4;
    uint quadNdcNumVec4s = 4;

    for (int j = 0; j < vec4ByteStride; j++) {
        out_data[gid * quadNdcNumVec4s + j] = in_data[old_index * quadNdcNumVec4s + j];
    }

    if (gid == 0) {
        drawElementsIndirectCommand.count        = 6;
        drawElementsIndirectCommand.instanceCount = u_count;
        drawElementsIndirectCommand.first        = 0;
        drawElementsIndirectCommand.baseInstance = 0;
    }
}
