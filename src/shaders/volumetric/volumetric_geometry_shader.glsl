#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

uniform vec2 metallicRoughnessFactors;
uniform vec3 inScale;

in VS_OUT {
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
    vec2 normalizedUv;
    vec3 scale;
} gs_in[];


out vec3 GaussianPosition;
flat out vec3 Scale;
out vec2 UV;
out vec4 Tangent;
out vec3 Normal;
flat out vec4 Quaternion;
uniform mat4 instanceMatrices[2];

//Copied from GLM
vec4 quat_cast(mat3 m) {
    float fourXSquaredMinus1 = m[0][0] - m[1][1] - m[2][2];
    float fourYSquaredMinus1 = m[1][1] - m[0][0] - m[2][2];
    float fourZSquaredMinus1 = m[2][2] - m[0][0] - m[1][1];
    float fourWSquaredMinus1 = m[0][0] + m[1][1] + m[2][2];

    int biggestIndex = 0;
    float fourBiggestSquaredMinus1 = fourWSquaredMinus1;
    if (fourXSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourXSquaredMinus1;
        biggestIndex = 1;
    }
    if (fourYSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourYSquaredMinus1;
        biggestIndex = 2;
    }
    if (fourZSquaredMinus1 > fourBiggestSquaredMinus1) {
        fourBiggestSquaredMinus1 = fourZSquaredMinus1;
        biggestIndex = 3;
    }

    float biggestVal = sqrt(fourBiggestSquaredMinus1 + 1.0f) * 0.5f;
    float mult = 0.25f / biggestVal;

    vec4 q;

    if (biggestIndex == 0) {
        q.w = biggestVal;
        q.x = (m[1][2] - m[2][1]) * mult;
        q.y = (m[2][0] - m[0][2]) * mult;
        q.z = (m[0][1] - m[1][0]) * mult;
    }
    else if (biggestIndex == 1) {
        q.w = (m[1][2] - m[2][1]) * mult;
        q.x = biggestVal;
        q.y = (m[0][1] + m[1][0]) * mult;
        q.z = (m[2][0] + m[0][2]) * mult;
    }
    else if (biggestIndex == 2) {
        q.w = (m[2][0] - m[0][2]) * mult;
        q.x = (m[0][1] + m[1][0]) * mult;
        q.y = biggestVal;
        q.z = (m[1][2] + m[2][1]) * mult;
    }
    else {
        q.w = (m[0][1] - m[1][0]) * mult;
        q.x = (m[2][0] + m[0][2]) * mult;
        q.y = (m[1][2] + m[2][1]) * mult;
        q.z = biggestVal;
    }

    return q;
}

vec3 project(vec3 v, vec3 u)
{
    float scalar = dot(v, u) / dot(u, u);
    return scalar * u;
}

void main() {
    //Gram Schmidt orthonormalization
    vec3 r1 = normalize(cross(gs_in[1].position - gs_in[0].position, gs_in[2].position - gs_in[0].position));
    vec3 m = (gs_in[0].position + gs_in[1].position + gs_in[2].position) / 3.0;
    vec3 r2 = normalize(gs_in[0].position - m);

    // To obtain r3, first get it as triangleVertices[1] - m
    vec3 initial_r3 = gs_in[1].position - m;

    // Gram-Schmidt Orthonormalization to find r3
    // Project initial_r3 onto r1
    // Project initial_r3 onto r1 and subtract it
    vec3 proj_r3_onto_r1 = project(initial_r3, r1);
    vec3 orthogonal_to_r1 = initial_r3 - proj_r3_onto_r1;

    // Project the result onto r2 and subtract it
    vec3 proj_r3_onto_r2 = project(orthogonal_to_r1, r2);
    vec3 orthogonal_to_r1_and_r2 = orthogonal_to_r1 - proj_r3_onto_r2;

    // Normalize the final result
    vec3 r3 = normalize(orthogonal_to_r1_and_r2);

    mat3 rotMat     = mat3(r2, r3, r1);
    vec4 q          = quat_cast(rotMat);
    vec4 quaternion = vec4(q.w, q.x, q.y, q.z);

    //SCALE
    Scale = gs_in[0].scale;

    for (int i = 0; i < 3; i++)
    {
        //All the rest
        Tangent = gs_in[i].tangent;
        GaussianPosition = gs_in[i].position;
        Normal = gs_in[i].normal;
        UV = gs_in[i].uv;
        Quaternion = quaternion;

        gl_Position = vec4(gs_in[i].normalizedUv * 2.0 - 1.0, 0.0, 1.0);
        EmitVertex();

    }
    EndPrimitive();
}

//Simple solution use instance_ID to offset

//Modern approach: f1) stream out -- append to buffer -- this would be the best solution!
//Modern way -- buffer and poke where you want, UAV (try going with this one)
