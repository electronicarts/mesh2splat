#version 460 core

layout(triangles) in;
layout(points, max_vertices = 1) out;

uniform vec3 inScale;

in TES_OUT {
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
} gs_in[];

out vec3 GaussianPosition;
out vec3 Scale;
out vec3 Normal;
out vec4 Quaternion;

void convertMat3ToFloat33(in mat3 mat_data, out float arr_data[3][3])
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            arr_data[i][j] = mat_data[i][j];
        }
    }
}

void convertFloat33ToMat3(in float arr_data[3][3], out mat3 mat_data)
{
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            mat_data[i][j] = arr_data[i][j];
        }
    }
}

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


void getEigenValuesVectors(in float mat_data[3][3], out float vectors[3][3], out float values[3])
{
    vec3 e = vec3(0);

    int dimension = 3;
    int dimensionMinusOne = 2;

    for (int j = 0; j < dimension; ++j)
        values[j] = mat_data[dimensionMinusOne][j];

    // Householder reduction to tridiagonal form.
    for (int i = dimensionMinusOne; i > 0 && i <= dimensionMinusOne; --i)
    {
        // Scale to avoid under/overflow.
        float scale = 0.0;
        float h = 0.0;
        for (int k = 0; k < i; ++k)
        {
            scale += abs(values[k]);
        }

        if (scale == 0.0)
        {
            e[i] = values[i - 1];

            for (int j = 0; j < i; ++j)
            {
                values[j] = mat_data[(i - 1)][j];
                mat_data[i][j] = 0.0;
                mat_data[j][i] = 0.0;
            }
        }
        else
        {
            // Generate Householder vector.
            for (int k = 0; k < i; ++k)
            {
                values[k] /= scale;
                h += values[k] * values[k];
            }

            float f = values[i - 1];
            float g = sqrt(h);

            if (f > 0.0)
            {
                g = -g;
            }

            e[i] = scale * g;
            h -= f * g;
            values[i - 1] = f - g;

            for (int j = 0; j < i; ++j)
            {
                e[j] = 0.0;
            }

            // Apply similarity transformation to remaining columns.
            for (int j = 0; j < i; ++j)
            {
                f = values[j];
                mat_data[j][i] = f;
                g = e[j] + mat_data[j][j] * f;

                for (int k = j + 1; k <= i - 1; ++k)
                {
                    g += mat_data[k][j] * values[k];
                    e[k] += mat_data[k][j] * f;
                }

                e[j] = g;
            }

            f = 0.0;
            for (int j = 0; j < i; ++j)
            {
                e[j] /= h;
                f += e[j] * values[j];
            }

            float hh = f / (h + h);

            for (int j = 0; j < i; ++j)
            {
                e[j] -= hh * values[j];
            }

            for (int j = 0; j < i; ++j)
            {
                f = values[j];
                g = e[j];

                for (int k = j; k <= i - 1; ++k)
                {
                    mat_data[k][j] = mat_data[k][j] - (f * e[k] + g * values[k]);
                }

                values[j] = mat_data[i - 1][j];
                mat_data[i][j] = 0.0;
            }
        }
        values[i] = h;
    }

    // Accumulate transformations.
    for (int i = 0; i < dimensionMinusOne; ++i)
    {
        mat_data[dimensionMinusOne][i] = mat_data[i][i];
        mat_data[i][i] = 1.0;
        float h = values[i + 1];

        if (h != 0.0)
        {
            for (int k = 0; k <= i; ++k)
            {
                values[k] = mat_data[k][i + 1] / h;
            }

            for (int j = 0; j <= i; ++j)
            {
                float g = 0.0;

                for (int k = 0; k <= i; ++k)
                {
                    g += mat_data[k][i + 1] * mat_data[k][j];
                }

                for (int k = 0; k <= i; ++k)
                {
                    mat_data[k][j] = mat_data[k][j] - (g * values[k]);
                }
            }
        }
        for (int k = 0; k <= i; ++k)
        {
            mat_data[k][i + 1] = 0.0;
        }
    }

    for (int j = 0; j < dimension; ++j)
    {
        values[j] = mat_data[dimensionMinusOne][j];
        mat_data[dimensionMinusOne][j] = 0.0;
    }

    mat_data[dimensionMinusOne][dimensionMinusOne] = 1.0;
    e[0] = 0.0;

    for (int i = 1; i < dimension; ++i)
        e[i - 1] = e[i];

    e[dimensionMinusOne] = 0.0;

    float f = float(0.0);
    float tst1 = float(0.0);
    float eps = float(pow(2.0, -52.0));
    for (int l = 0; l < dimension; ++l)
    {
        // Find small subdiagonal element
        tst1 = float(max(tst1, abs(values[l]) + abs(e[l])));
        int m = l;
        while (m < dimension)
        {
            if (abs(e[m]) <= eps * tst1) break;
            ++m;
        }

        // If m == l, d[l] is an eigenvalue,
        // otherwise, iterate.
        if (m > l && l < 2)
        {
            int iter = 0;
            do
            {
                ++iter;  // (Could check iteration count here.)
                // Compute implicit shift
                float g = values[l];
                float p = (values[l + 1] - g) / (float(2.0) * e[l]);
                float r = float(sqrt(p * p + float(1.0) * float(1.0)));
                if (p < 0) r = -r;
                values[l] = e[l] / (p + r);
                values[l + 1] = e[l] * (p + r);
                float dl1 = values[l + 1];
                float h = g - values[l];
                for (int i = l + 2; i < dimension; ++i)
                    values[i] -= h;
                f = f + h;

                // Implicit QL transformation.
                p = values[m];
                float c = float(1.0);
                float c2 = c;
                float c3 = c;
                float el1 = e[l + 1];
                float s = float(0.0);
                float s2 = float(0.0);
                for (int i = m - 1; i >= l && i <= m - 1; --i)
                {
                    c3 = c2;
                    c2 = c;
                    s2 = s;
                    g = c * e[i];
                    h = c * p;
                    r = float(sqrt(p * p + e[i] * e[i]));
                    e[i + 1] = s * r;
                    s = e[i] / r;
                    c = p / r;
                    p = c * values[i] - s * g;
                    values[i + 1] = h + s * (c * g + s * values[i]);

                    // Accumulate transformation.
                    for (int k = 0; k < dimension; ++k)
                    {
                        h = mat_data[k][i + 1];
                        mat_data[k][i + 1] = (s * mat_data[k][i] + c * h);
                        mat_data[k][i] = (c * mat_data[k][i] - s * h);
                    }
                }

                p = -s * s2 * c3 * el1 * e[l] / dl1;
                e[l] = s * p;
                values[l] = c * p;
                // Check for convergence.
            } while (abs(e[l]) > eps * tst1 && iter < 30);
        }
        values[l] = values[l] + f;
        e[l] = float(0.0);
    }

    // Sort eigenvalues and corresponding vectors.
    for (int i = 0; i < dimensionMinusOne; ++i)
    {
        int k = i;
        float p = values[i];

        for (int j = i + 1; j < dimension; ++j)
        {
            if (values[j] < p)
            {
                k = j;
                p = values[j];
            }
        }
        if (k != i)
        {
            values[k] = values[i];
            values[i] = p;
            for (int j = 0; j < dimension; ++j)
            {
                p = mat_data[j][i];
                mat_data[j][i] = mat_data[j][k];
                mat_data[j][k] = p;
            }
        }
    }

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            vectors[i][j] = mat_data[i][j];
}

//Taken from Frostbite
mat3 inverse(mat3 m)
{
	float n11 = m[0][0], n12 = m[0][1], n13 = m[0][2];
	float n21 = m[1][0], n22 = m[1][1], n23 = m[1][2];
	float n31 = m[2][0], n32 = m[2][1], n33 = m[2][2];

	float t11 = n22 * n33 - n23 * n32;
	float t12 = n23 * n31 - n21 * n33;
	float t13 = n21 * n32 - n22 * n31;

	float det = n11 * t11 + n12 * t12 + n13 * t13;
	float idet = 1.0f / det;

	mat3 ret;

	ret[0][0] = t11 * idet;
	ret[0][1] = (n13 * n32 - n12 * n33) * idet;
	ret[0][2] = (n12 * n23 - n13 * n22) * idet;
	ret[1][0] = t12 * idet;
	ret[1][1] = (n11 * n33 - n13 * n31) * idet;
	ret[1][2] = (n13 * n21 - n11 * n23) * idet;
	ret[2][0] = t13 * idet;
	ret[2][1] = (n12 * n31 - n11 * n32) * idet;
	ret[2][2] = (n11 * n22 - n12 * n21) * idet;

	return ret;
}

//Also taken from Frostbite
mat2 inverse(mat2 m)
{
	float a = m[0][0], b = m[0][1], c = m[1][0], d = m[1][1];
	return (1.0 / (a*d - b*c))*mat2(d, -b, -c, a);
}

vec3 interpolateWithBaryCoord(vec3 barycentric, vec3 v1, vec3 v2, vec3 v3) {
    return barycentric.x * v1 + barycentric.y * v2 + barycentric.z * v3;
}

mat3x2 multiply3x2With2x2(mat3x2 VMatrix, mat2 UVMatrix) {
    // Expand the 3x2 matrix VMatrix to a 3x3 matrix by adding a column of zeros
    mat3 VMatrixExpanded = mat3(
        vec3(VMatrix[0], 0.0),
        vec3(VMatrix[1], 0.0),
        vec3(VMatrix[2], 0.0)
    );

    // Expand the 2x2 matrix UVMatrix to a 3x3 matrix by adding a row and a column of zeros
    mat3 UVMatrixExpanded = mat3(
        vec3(UVMatrix[0], 0.0),
        vec3(UVMatrix[1], 0.0),
        vec3(0.0, 0.0, 1.0)
    );

    // Perform the multiplication of the expanded matrices
    mat3 result = VMatrixExpanded * UVMatrixExpanded;

    // Extract the relevant 3x2 part of the result matrix
    mat3x2 finalResult = mat3x2(
        result[0].xy,
        result[1].xy,
        result[2].xy
    );

    return finalResult;
}


// Placeholder function for Jacobian computation
/*
mat2x3 computeUv3DJacobian(mat3 vertices, mat3x2 uvs) {
    // Compute edge vectors in UV space
    vec2 edgeUV1 = uvs[1] - uvs[0];
    vec2 edgeUV2 = uvs[2] - uvs[0];

    // Compute edge vectors in 3D space
    vec3 edge3D1 = vertices[1] - vertices[0];
    vec3 edge3D2 = vertices[2] - vertices[0];

    // Define the 2x2 UV transformation matrix
    mat2 UVMatrix = mat2(edgeUV1.x, edgeUV2.x,
                        edgeUV1.y, edgeUV2.y);

    // Define the 3x2 vertex coordinate matrix
    mat3x2 VMatrix = mat3x2(edge3D1.x, edge3D2.x,
                            edge3D1.y, edge3D2.y,
                            edge3D1.z, edge3D2.z);

    return VMatrix * inverse(UVMatrix);

}*/

// Placeholder function for eigen decomposition
vec2 getTwoLargestEigenvalues(mat3 covMat) {
    float covMatInArrayForm[3][3];
    float eigenvectors[3][3];
    float eigenvalues[3];
    convertMat3ToFloat33(covMat, covMatInArrayForm);
    //Eigenvectors and eigenvalues are in ascending order
    getEigenValuesVectors(covMatInArrayForm, eigenvectors, eigenvalues);

    return vec2(eigenvalues[2], eigenvalues[1]); 
}

float triangleArea(float A, float B, float C)
{
    float s = (A + B + C) / 2;
    return sqrt( s * (s - A) * (s - B) * (s - C));
}

vec3 project(vec3 v, vec3 u)
{
    float scalar = dot(v, u) / dot(u, u);
    return scalar * u;
}

float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec2[2] computeUVBoundingBox(vec2 triangleUVs[3]) {
    float minU = triangleUVs[0].x, maxU = triangleUVs[0].x;
    float minV = triangleUVs[0].y, maxV = triangleUVs[0].y;

    for (int i = 0; i < 3; i++) {
        vec2 uv = triangleUVs[i];
        minU = min(minU, uv.x);
        maxU = max(maxU, uv.x);

        minV = min(minV, uv.y);
        maxV = max(maxV, uv.y);
    }

    // Clamp values to [0, 1]
    minU = max(minU, 0.0);
    maxU = min(maxU, 1.0);
    minV = max(minV, 0.0);
    maxV = min(maxV, 1.0);

    return vec2[2](vec2(minU, minV), vec2(maxU, maxV));
}

ivec2 uvToPixel(vec2 uv, int textureWidth, int textureHeight) {
    // Convert back to pixel coordinates
    int x = int(uv.x * float(textureWidth) - 0.5);
    int y = int(uv.y * float(textureHeight) - 0.5); // Remember to swap the order of the pixels in the rgba_to_pos

    // Clamp the coordinates to ensure they're within the texture bounds
    // This accounts for potential rounding errors that might place the pixel outside the texture
    x = clamp(x, 0, textureWidth - 1);
    y = clamp(y, 0, textureHeight - 1);

    return ivec2(x, y);
}

vec2 pixelToUV(ivec2 pixel, int textureWidth, int textureHeight) {
    // Convert pixel coordinates to normalized UV coordinates by dividing
    // by the texture dimensions. Cast to float to ensure floating-point division.
    float u = float(pixel.x) / float(textureWidth);
    float v = float(pixel.y) / float(textureHeight);

    return vec2(u, v);
}

float sign(vec2 p1, vec2 p2, vec2 p3) {
    return (p1.x - p3.x) * (p2.y - p3.y) - (p2.x - p3.x) * (p1.y - p3.y);
}

bool pointInTriangle(vec2 pt, vec2 v1, vec2 v2, vec2 v3) {
    float d1 = sign(pt, v1, v2);
    float d2 = sign(pt, v2, v3);
    float d3 = sign(pt, v3, v1);

    bool hasNeg = (d1 < 0.0) || (d2 < 0.0) || (d3 < 0.0);
    bool hasPos = (d1 > 0.0) || (d2 > 0.0) || (d3 > 0.0);

    return !(hasNeg && hasPos);
}

bool computeBarycentricCoords(vec2 p, vec2 a, vec2 b, vec2 c, out float u, out float v, out float w) {
    vec2 v0 = b - a;
    vec2 v1 = c - a;
    vec2 v2 = p - a;
    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);
    float denom = d00 * d11 - d01 * d01;
    if (denom == 0.0) return false; // Collinear or singular triangle
    float invDenom = 1.0f / denom;
    v = (d11 * d20 - d01 * d21) * invDenom;
    w = (d00 * d21 - d01 * d20) * invDenom;
    u = 1.0 - v - w;
    return true; // Successfull computation
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

    mat3 rotMat = mat3( r2, r3, r1 );
    vec4 q = quat_cast(rotMat);
    vec4 quaternion = vec4(q.w, q.x, q.y, q.z);

    /*
    float Sd_xy = min(length(gs_in[0].uv - gs_in[1].uv), min(length(gs_in[0].uv - gs_in[2].uv), length(gs_in[2].uv - gs_in[1].uv)));
    float Sd_z  = Sd_xy / 10.0f;

    float s_xy  = log(sqrt(Sd_xy));
    float s_z   = log(Sd_z);

    vec3 scaling = 0.5 * vec3(s_xy, s_xy, s_z);
    */

    vec3 bc[3] = {
        vec3((3 - sqrt(3)) / 6, (3 - sqrt(3)) / 6, sqrt(3) / 3),
        vec3((3 - sqrt(3)) / 6, sqrt(3) / 3, (3 - sqrt(3) / 6)),
        vec3(sqrt(3) / 3, (3 - sqrt(3)) / 6, (3 - sqrt(3)) / 6),
    };
    //NB: As soon as get this fixed --> varying variable, just need to compute at the vertices and pass to rasterizer

    GaussianPosition    = gs_in[0].position;// * u + gs_in[1].position * v + gs_in[2].position * w;
    Normal              = gs_in[0].normal;//   * u + gs_in[1].normal   * v + gs_in[2].normal   * w; //pass tangent and here recover normal from normal map
    Quaternion          = quaternion;
    Scale               = inScale;

    EmitVertex();
    EndPrimitive();
}
