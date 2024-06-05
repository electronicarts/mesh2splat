#version 460 core

layout(triangles) in;
layout(line_strip, max_vertices = 40) out;

in VS_OUT{
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

// Copied from GLM
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

vec3 project(vec3 v, vec3 u) {
    float scalar = dot(v, u) / dot(u, u);
    return scalar * u;
}

float rand(float n) { return fract(sin(n) * 43758.5453123); }

void main() {
    // Gram-Schmidt orthonormalization ------------------------------
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

    mat3 rotMat = mat3(r2, r3, r1);
    vec4 q = quat_cast(rotMat);
    vec4 quaternion = vec4(q.w, q.x, q.y, q.z);
    //-------------------------------------------------------------
    //Random barycentric coordinates
    int numberOfStrains = int(mix(10, 20, rand(gl_PrimitiveIDIn * 100)));

    float currentLastRandomNumber = 0;

    for (int i = 0; i < numberOfStrains; i++)
    {
        float bx = rand(gl_InvocationID + i);
        float by = rand(gl_InvocationID + bx * 20);
        float bz = 1 - bx - by;

        vec3 avgPosition                = (gs_in[0].position * bx + gs_in[1].position * by  + gs_in[2].position * bz);
        vec2 avgUV                      = (gs_in[0].uv * bx + gs_in[1].uv * by + gs_in[2].uv * bz);
        vec2 normalizedUvStartingPoint  = gs_in[0].normalizedUv;

        // First vertex
        float s_xy      = log(exp(gs_in[0].scale.x) / 2.5);
        float s_z       = log(exp(gs_in[0].scale.x) * 3.5);
        float edgeLen   = length(gs_in[0].position - gs_in[1].position);

        Scale               = vec3(s_xy, s_xy, s_z);
        Tangent             = (gs_in[0].tangent * bx + gs_in[1].tangent * by + gs_in[2].tangent * bz);
        GaussianPosition    = avgPosition;
        Normal              = r1;
        UV                  = vec2(gl_PrimitiveIDIn, gl_PrimitiveIDIn);
        Quaternion          = quaternion;

        gl_Position         = vec4(normalizedUvStartingPoint * 2.0 - 1.0, 0.0, 1.0);
        EmitVertex();

        // Second vertex
        GaussianPosition                += r1 * (edgeLen * 2);
        vec2 normalizedUvFinalPoint     = gs_in[1].normalizedUv;

        gl_Position = vec4(normalizedUvFinalPoint * 2.0 - 1.0, 0.0, 1.0);
        EmitVertex();


        EndPrimitive();
    }


    
}
