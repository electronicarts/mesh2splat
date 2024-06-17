#version 460 core

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

uniform vec2 metallicRoughnessFactors;
uniform float sigma_x;
uniform float sigma_y;


// Match this struct with the VS_OUT struct from the vertex shader
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


void transpose2x3(in mat2x3 m, out mat3x2 outputMat) {
    //mat3x2 transposed;
    outputMat[0][0] = m[0][0];
    outputMat[1][0] = m[0][1];
    outputMat[2][0] = m[0][2];

    outputMat[0][1] = m[1][0];
    outputMat[1][1] = m[1][1];
    outputMat[2][1] = m[1][2];
}

vec4 quat_mult(vec4 q1, vec4 q2) {
    return vec4(
        q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        q1.w * q2.y + q1.y * q2.w + q1.z * q2.x - q1.x * q2.z,
        q1.w * q2.z + q1.z * q2.w + q1.x * q2.y - q1.y * q2.x,
        q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z
    );
}

vec4 quat_normalize(vec4 q) {
    return q / length(q);
}

//q = (x,y,z,w) some might write it as  w,x,y,z
mat3 mat3_cast(vec4 q) {
   mat3 rotMat;
   float e0 = q[3];
   float e1 = q[0];
   float e2 = q[1];
   float e3 = q[2];

   float e0_square = e0 * e0;
   float e1_square = e1 * e1;
   float e2_square = e2 * e2;
   float e3_square = e3 * e3;

   float e1xe2 = e1 * e2;
   float e0xe2 = e0 * e2;
   float e0xe3 = e0 * e3;
   float e1xe3 = e1 * e3;
   float e2xe3 = e2 * e3;
   float e0xe1 = e0 * e1;

   rotMat[0][0] = e0_square + e0_square - e1_square - e2_square;
   rotMat[1][0] = (2 * e1xe2) - (2 * e0xe3);
   rotMat[2][0] = (2 * e0xe2) + (2 * e1xe3);

   rotMat[0][1] = (2 * e0xe3) + (2 * e1xe2);
   rotMat[1][1] = e0_square - e1_square + e2_square - e3_square;
   rotMat[2][1] = (2 * e0xe1) - (2 * e2xe3);

   rotMat[0][2] = (2 * e1xe3) - (2 * e0xe2);
   rotMat[1][2] = (2 * e0xe1) + (2 * e2xe3);
   rotMat[2][2] = e0_square - e1_square - e2_square + e3_square;

   return rotMat;
}

vec4 Diagonalizer(mat3 A, out vec3 outDiagonal) {
    int maxsteps = 30;
    vec4 q = vec4(0.0, 0.0, 0.0, 1.0);

    for (int i = 0; i < maxsteps; i++) {
        mat3 Q = mat3_cast(q);
        mat3 D = transpose(Q) * A * Q;
        vec3 offdiag = vec3(D[2][1], D[2][0], D[1][0]);
        vec3 om = abs(offdiag);

        outDiagonal = vec3(D[0][0], D[1][1], D[2][2]);

        int k = (om.x > om.y && om.x > om.z) ? 0 : (om.y > om.z) ? 1 : 2;
        int k1 = (k + 1) % 3;
        int k2 = (k + 2) % 3;
        if (offdiag[k] == 0.0)
            break;
        float thet = (D[k2][k2] - D[k1][k1]) / (2.0 * offdiag[k]);
        float sgn = (thet > 0.0) ? 1.0 : -1.0;
        thet *= sgn;
        float t = sgn / (thet + ((thet < 1.0e6) ? sqrt(thet * thet + 1.0) : thet));
        float c = 1.0 / sqrt(t * t + 1.0);
        if (c == 1.0)
            break;
        vec4 jr = vec4(0.0);
        jr[k] = sgn * sqrt((1.0 - c) / 2.0);
        jr[k] *= -1.0;
        jr.w = sqrt(1.0 - (jr[k] * jr[k]));
        if (jr.w == 1.0)
            break;
        q = quat_mult(q, jr);
        q = quat_normalize(q);
    }
    return q;
}


//-------------------------------------

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

mat2 construct2DCovMatrix(float sigmaX, float sigmaY)
{
    mat2 covMatrix;

    // Set the variances
    covMatrix[0][0] = sigmaX * sigmaX; // Variance in the X direction
    covMatrix[1][1] = sigmaY * sigmaY; // Variance in the Y direction

    // Set the covariances to 0
    covMatrix[0][1] = 0.0;
    covMatrix[1][0] = 0.0;

    return covMatrix;
}

vec3 computeBarycentric2D(vec2 p, vec2 a, vec2 b, vec2 c)
{
    vec2 v0 = b - a;
    vec2 v1 = c - a;
    vec2 v2 = p - a;
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float inv_det = 1.0 / (d00 * d11 - d01 * d01);
    float v = (d11 * d20 - d01 * d21) * inv_det;
    float w = (d00 * d21 - d01 * d20) * inv_det;
    float u = 1.0 - v - w;
    return vec3(u, v, w);
}

mat2 inverse2x2(mat2 m) {
    float determinant = m[0][0] * m[1][1] - m[0][1] * m[1][0];
    if (determinant == 0.0) {
        return mat2(0.0); // Handle non-invertible case if necessary
    }
    float invDet = 1.0 / determinant;
    mat2 inverse;
    inverse[0][0] = m[1][1] * invDet;
    inverse[1][0] = -m[1][0] * invDet;

    inverse[0][1] = -m[0][1] * invDet;
    inverse[1][1] = m[0][0] * invDet;

    return inverse;
}

//2x3 means 2 columns and 3 rows (in normal notation it would be 3x2 * 2x2)  ------------------- checked
mat2x3 multiplyMat2x3WithMat2x2(mat2x3 matA, mat2 matB) {
    mat2x3 result;

    result[0][0] = matA[0][0] * matB[0][0] + matA[1][0] * matB[0][1];
    result[1][0] = matA[0][0] * matB[1][0] + matA[1][0] * matB[1][1];

    result[0][1] = matA[0][1] * matB[0][0] + matA[1][1] * matB[0][1];
    result[1][1] = matA[0][1] * matB[1][0] + matA[1][1] * matB[1][1];

    result[0][2] = matA[0][2] * matB[0][0] + matA[1][2] * matB[0][1];
    result[1][2] = matA[0][2] * matB[1][0] + matA[1][2] * matB[1][1];

    return result;
}

mat3x2 multiplyMat2x2WithMat3x2(mat2 matA, mat3x2 matB) {
    mat3x2 result;

    result[0][0] = matA[0][0] * matB[0][0] + matA[1][0] * matB[0][1];
    result[1][0] = matA[0][0] * matB[1][0] + matA[1][0] * matB[1][1];
    result[2][0] = matA[0][0] * matB[2][0] + matA[1][0] * matB[2][1];

    result[0][1] = matA[0][1] * matB[0][0] + matA[1][1] * matB[0][1];
    result[1][1] = matA[0][1] * matB[1][0] + matA[1][1] * matB[1][1];
    result[2][1] = matA[0][1] * matB[2][0] + matA[1][1] * matB[2][1];

    return result;
}

mat3 multiplyMat2x3WithMat3x2(mat2x3 matA, mat3x2 matB) {
    mat3 result;

    // Perform the matrix multiplication
    result[0][0] = matA[0][0] * matB[0][0] + matA[1][0] * matB[0][1];
    result[1][0] = matA[0][0] * matB[1][0] + matA[1][0] * matB[1][1];
    result[2][0] = matA[0][0] * matB[2][0] + matA[1][0] * matB[2][1];

    result[0][1] = matA[0][1] * matB[0][0] + matA[1][1] * matB[0][1];
    result[1][1] = matA[0][1] * matB[1][0] + matA[1][1] * matB[1][1];
    result[2][1] = matA[0][1] * matB[2][0] + matA[1][1] * matB[2][1];

    result[0][2] = matA[0][2] * matB[0][0] + matA[1][2] * matB[0][1];
    result[1][2] = matA[0][2] * matB[1][0] + matA[1][2] * matB[1][1];
    result[2][2] = matA[0][2] * matB[2][0] + matA[1][2] * matB[2][1];

    return result;
}

mat2x3 computeUv3DJacobian(vec3 verticesTriangle3D[3], vec2 verticesTriangleUV[3]) {
    // 3D positions of the triangle's vertices
    vec3 pos0 = verticesTriangle3D[0];
    vec3 pos1 = verticesTriangle3D[1];
    vec3 pos2 = verticesTriangle3D[2];

    // UV coordinates of the triangle's vertices
    vec2 uv0 = verticesTriangleUV[0];
    vec2 uv1 = verticesTriangleUV[1];
    vec2 uv2 = verticesTriangleUV[2];

    mat2 UVMatrix;
    UVMatrix[0][0] = uv1.x - uv0.x;
    UVMatrix[1][0] = uv2.x - uv0.x;

    UVMatrix[0][1] = uv1.y - uv0.y;
    UVMatrix[1][1] = uv2.y - uv0.y;


    mat2x3 VMatrix;
    VMatrix[0][0] = pos1.x - pos0.x;
    VMatrix[1][0] = pos2.x - pos0.x;

    VMatrix[0][1] = pos1.y - pos0.y;
    VMatrix[1][1] = pos2.y - pos0.y;

    VMatrix[0][2] = pos1.z - pos0.z;
    VMatrix[1][2] = pos2.z - pos0.z;

    // Compute the Jacobian matrix
    return multiplyMat2x3WithMat2x2(VMatrix, inverse2x2(UVMatrix));
}

vec3 sortDescending(vec3 v) {
    float temp;

    if (v.x < v.y) {
        temp = v.x;
        v.x = v.y;
        v.y = temp;
    }

    if (v.x < v.z) {
        temp = v.x;
        v.x = v.z;
        v.z = temp;
    }

    if (v.y < v.z) {
        temp = v.y;
        v.y = v.z;
        v.z = temp;
    }

    return v;
}

vec4 gramSchmidtOrthonormalization()
{
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
    return vec4(q.w, q.x, q.y, q.z);
}

void main() {
    vec4 quaternion = gramSchmidtOrthonormalization();//vec4(q.w, q.x, q.y, q.z);

    //SCALE
    mat2 cov2d = construct2DCovMatrix(sigma_x, sigma_y);
    //This matrix can be constructed on CPU 
    mat2x3 J;

    if (false)
    {
        vec2 trianglePixel2D[3];
        trianglePixel2D[0] = vec2(0.0f, 0.0f);
        trianglePixel2D[1] = vec2(sigma_x, 0.0f);
        trianglePixel2D[2] = vec2(0.0f, sigma_y);

        vec3 pixel3DTriangle[3];
        for (int i = 0; i < 3; i++)
        {
            vec3 barycentricCoords = computeBarycentric2D(trianglePixel2D[i], gs_in[0].normalizedUv, gs_in[1].normalizedUv, gs_in[2].normalizedUv);
            pixel3DTriangle[i] = barycentricCoords.x * gs_in[0].position + barycentricCoords.y * gs_in[1].position + barycentricCoords.z * gs_in[2].position;
        }

        J = computeUv3DJacobian(pixel3DTriangle, trianglePixel2D);
    }
    else
    {
        vec3 true_vertices3D[3] = { gs_in[0].position, gs_in[1].position, gs_in[2].position };
        vec2 true_normalied_vertices2D[3] = { gs_in[0].normalizedUv, gs_in[1].normalizedUv, gs_in[2].normalizedUv };

        J = computeUv3DJacobian(true_vertices3D, true_normalied_vertices2D);
    }
  
    mat3x2 J_T;
    transpose2x3(J, J_T);
    mat3 cov3d = multiplyMat2x3WithMat3x2(J, multiplyMat2x2WithMat3x2(cov2d, J_T));

    vec3 diagonalizedCov3d_diagonal;
    vec4 quatRot = Diagonalizer(cov3d, diagonalizedCov3d_diagonal);

    //Not entirely sure about the sorting part, maybe I should not sort them(?)
    vec3 sortedEigenvalues = sortDescending(diagonalizedCov3d_diagonal);
    //float avg_scale = sqrt((sortedEigenvalues[0] + sortedEigenvalues[1]) / 2.0f);// +sqrt(sortedEigenvalues[1])) / 2.0f;
    float s_x = sqrt(sortedEigenvalues[0]); //avg_scale;
    float s_y = sqrt(sortedEigenvalues[0]); //avg_scale;

    float packed_s_x    = log(s_x);
    float packed_s_y    = log(s_y);

    float packed_s_z    = log(1e-7);

    Scale = vec3(packed_s_x, packed_s_y, packed_s_z);

    for (int i = 0; i < 3; i++)
    {
        Tangent                 = gs_in[i].tangent;       
        GaussianPosition        = gs_in[i].position;
        Normal                  = gs_in[i].normal;
        UV                      = gs_in[i].uv;
        Quaternion              = quaternion;
        //-------------------------------------------------------------------------
        gl_Position             = vec4(gs_in[i].normalizedUv * 2.0 - 1.0, 0.0, 1.0);
        EmitVertex();
    }
    EndPrimitive();
}
