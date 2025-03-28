///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#version 460 core

layout(std430, binding = 0) buffer GaussiansIn {
    float depths[];
};

layout(std430, binding = 1) buffer Keys {
    uint key[];
};

layout(std430, binding = 2) buffer Values {
    uint val[];
};

uniform uint  u_count; 

layout(local_size_x = 16, local_size_y = 16) in;
void main() {
	uint globalWidth = gl_NumWorkGroups.x * gl_WorkGroupSize.x;
    uint gid = gl_GlobalInvocationID.y * globalWidth + gl_GlobalInvocationID.x;
    
    if (gid >= u_count) return;

    uint bits = floatBitsToUint(depths[gid]);

    key[gid] = bits; 
    val[gid] = gid;
}
