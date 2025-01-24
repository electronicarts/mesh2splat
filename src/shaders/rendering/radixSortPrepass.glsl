#version 460 core

layout(std430, binding = 0) buffer GaussiansIn {
    vec4 data[]; // data[ i*6 + 0 ] = position, data[ i*6 + 1 ] = color, ...
};

layout(std430, binding = 1) buffer Keys {
    uint key[];
};

layout(std430, binding = 2) buffer Values {
    uint val[];
};

uniform mat4 u_view; 
uniform uint  u_count; 

layout(local_size_x = 256) in;
void main() {
    uint i = gl_GlobalInvocationID.x;
    if (i >= u_count) return;
    vec4 pos_world = data[i*6 + 0];  
    vec4 pos_view  = u_view * pos_world;
    float depth    = pos_view.z; //Sign changes sorting order     
    uint bits = floatBitsToUint(depth);

    key[i] = bits; 
    val[i] = i;
}
