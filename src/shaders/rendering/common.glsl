///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

// Shared utility functions for mesh and gaussian splat shading.
// Do NOT add a #version directive here.

#ifndef COMMON_GLSL
#define COMMON_GLSL

//https://github.com/jagracar/webgl-shader-examples/blob/master/shaders/requires/random2d.glsl
float random2d(vec2 co) {
    highp float a = 12.9898;
    highp float b = 78.233;
    highp float c = 43758.5453;
    highp float dt = dot(co.xy, vec2(a, b));
    highp float sn = mod(dt, 3.14);
    return fract(sin(sn) * c);
}

void castQuatToMat3(vec4 quat, out mat3 rotMatrix)
{
	vec3 firstRow = vec3(
		1.f - 2.f * (quat.z * quat.z + quat.w * quat.w),
		2.f * (quat.y * quat.z - quat.x * quat.w),      
		2.f * (quat.y * quat.w + quat.x * quat.z)       
	);

	vec3 secondRow = vec3(
		2.f * (quat.y * quat.z + quat.x * quat.w),       
		1.f - 2.f * (quat.y * quat.y + quat.w * quat.w), 
		2.f * (quat.z * quat.w - quat.x * quat.y)        
	);

	vec3 thirdRow = vec3(
		2.f * (quat.y * quat.w - quat.x * quat.z),       
		2.f * (quat.z * quat.w + quat.x * quat.y),     
		1.f - 2.f * (quat.y * quat.y + quat.z * quat.z) 
	);

	rotMatrix = mat3(
		firstRow,
		secondRow,
		thirdRow
	);
}

void computeCov3D(mat3 rotMat, vec3 scales, out mat3 sigma3d) {

	mat3 scaleMatrix = mat3(
		scales.x , 0, 0, 
		0, scales.y , 0,
		0, 0, scales.z 					
	);

	mat3 mMatrix = scaleMatrix * rotMat;

	sigma3d = transpose(mMatrix) * mMatrix;
}

mat2 inverseMat2(mat2 m)
{
    float det = m[0][0] * m[1][1] - m[0][1] * m[1][0];
	mat2 inv;

	if (det != 0)
	{
		inv[0][0] =  m[1][1] / det;
		inv[0][1] = -m[0][1] / det;
		inv[1][0] = -m[1][0] / det;
		inv[1][1] =  m[0][0] / det;
	}
	else
		inv = mat2(0.0);

    return inv;
}

float computeExponentialDepth(float viewDepth, vec2 nearFar) {
    float normalizedDepth = (viewDepth - nearFar.x) / (nearFar.y - nearFar.x);
    float invertedLinearizedDepth = clamp(normalizedDepth, 0, 1);
    return clamp(exp(-20.0 * invertedLinearizedDepth), 0, 1);
}

vec3 encodeNormal(vec3 normal) {
    return normal * 0.5 + 0.5;
}

vec3 decodeNormal(vec3 encodedNormal) {
    return encodedNormal * 2.0 - 1.0;
}

#endif
