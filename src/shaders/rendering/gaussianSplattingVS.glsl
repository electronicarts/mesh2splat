#version 460 core

#define ALPHA_CUTOFF (1.0f/256.0f)
#define GAUSSIAN_CUTOFF_SCALE (sqrt(log(1.0f / ALPHA_CUTOFF)))
#define MIN_PIXEL_RADIUS 1.0f

// Static quad vertex position per-vertex
layout(location = 0) in vec3 vertexPos;

// Per-instance attributes
layout(location = 1) in vec4 gaussianPosition_ms; //model space  
layout(location = 2) in vec4 gaussianColor;
layout(location = 3) in vec4 gaussianPackedScale;
layout(location = 4) in vec4 gaussianQuaternion;
layout(location = 5) in vec4 gaussianPbr;

// You can add more per-instance attributes at locations 3, 4, 5 as needed.
out vec3 out_color;
out vec2 uv;

uniform mat4 u_MVP;
uniform mat4 u_worldToView;	
uniform mat4 u_viewToClip;	
uniform mat4 u_objectToWorld; 
uniform vec2 u_resolution;

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

   return rotMat; //TODO: is inout
}

vec3 computeConic(mat4 objectToWorld, mat4 worldToView, mat4 viewToClip, vec2 resolution, out float alphaScale)
{
	// just reading 3 floats
	vec3 wsCenterPos	= (vec4(0, 0, 0, 1) * objectToWorld).xyz;
	vec4 vsCenterPosHom = vec4(wsCenterPos, 1) * worldToView;
	vec3 vsCenterPos	= vsCenterPosHom.xyz;

	// covariance Matrix in world space, symmetric, can be optimized
	mat3 wsBigSigma = (transpose(mat3(objectToWorld))) * mat3(objectToWorld);
	//float3x3 wsBigSigma = mul((float3x3) objectToWorld, transpose((float3x3) objectToWorld));

	// covariance Matrix in view space, symmetric, can be optimized
	mat3 W = mat3(worldToView);

	mat3 vsBigSigma = transpose(W) * wsBigSigma * W;

	//mat3 vsBigSigma = mul(W, mul(wsBigSigma, transpose(W)));

	// https://stackoverflow.com/questions/46075606/getting-focal-length-and-focal-point-from-a-projection-matrix#:~:text=The%20focal%20length%20is%20merely,to%20obtain%20the%20focal%20point.
	// The focal length is merely the first element in the matrix (m11).
	//float2 focalLength = float2(viewToClip._m11, viewToClip._m11);

	vec2 focalLength = vec2(viewToClip[1][1], viewToClip[1][1]);

	// the following lines are copied from https://github.com/aras-p/UnityGaussianSplatting/blob/main/Assets/GaussianSplatting/Shaders/GaussianSplatting.hlsl
	float aspect = viewToClip[0][0] / viewToClip[1][1];
	
	float tanFovX = 1 / viewToClip[0][0];
	float tanFovY = 1 / viewToClip[1][1];
	float limX = 1.3 * tanFovX;
	float limY = 1.3 * tanFovY;
	vsCenterPos.x = clamp(vsCenterPos.x / vsCenterPos.z, -limX, limX) * vsCenterPos.z;
	vsCenterPos.y = clamp(vsCenterPos.y / vsCenterPos.z, -limY, limY) * vsCenterPos.z;

	// Jacobian of perspective matrix = affine approximation at location csCenterPos
	// we only need 2x2 part
	mat3 J = mat3(
		focalLength.x / vsCenterPos.z, 0, 0,
		0, focalLength.y / vsCenterPos.z, 0,
		-(focalLength.x * vsCenterPos.x) / (vsCenterPos.z * vsCenterPos.z),	-(focalLength.y * vsCenterPos.y) / (vsCenterPos.z * vsCenterPos.z), 0);
	
	mat2 csBigSigma = mat2(transpose(J) * vsBigSigma * J);
	csBigSigma[0][0] += MIN_PIXEL_RADIUS * MIN_PIXEL_RADIUS / (resolution.y * resolution.y);
	csBigSigma[1][1] += MIN_PIXEL_RADIUS * MIN_PIXEL_RADIUS / (resolution.y * resolution.y);


	float before = determinant(csBigSigma);

	// Apply low-pass filter: every Gaussian should be at least
	// one pixel wide/high. Discard 3rd row and column.
	//csBigSigma._m00 += coc * coc / (resolution.y * resolution.y);
	//csBigSigma._m11 += coc * coc / (resolution.y * resolution.y);

	float after = determinant(csBigSigma);

	// larger splats should get fainter
	alphaScale = before / after;

	// result is symmetrical so it can be stored in 3 coefficients, one coefficient is there twice => *2
	mat2 invCov = inverse(mat2(csBigSigma));

	return vec3(invCov[0][0], 2 * invCov[1][0], invCov[1][1]);
}

vec2 computeCorner(vec2 xy, vec3 conic, vec2 resolution)
{
	float a = conic.x;
	float b = 0.5f * conic.y;
	float c = conic.z;

	//https://www.shadertoy.com/view/msGcDh
	float baseHalf = (a + c) * 0.5f;
	float rootHalf = 0.5f * sqrt((a - c) * (a - c) + 4.0f * b * b);
	float rx = inversesqrt(baseHalf + rootHalf);
	float ry = inversesqrt(baseHalf - rootHalf);

	vec2 k = vec2(a - c, 2.0f * b);

	// if splat gets thinner than a pixel, keep it pixel size for antialiasing, this works with aspectRatio
	rx = max(rx, 1.0f / resolution.y);
	ry = max(ry, 1.0f / resolution.x);

	// half vector, should be faster than atan()
	vec2 axis0 = normalize(k + vec2(length(k), 0));

	vec2 axis1 = vec2(axis0.y, -axis0.x);

	return (axis0 * (rx * xy.x) - axis1 * (ry * xy.y)) * vec2(resolution.y / resolution.x, 1.0f);
}

mat4 calcMatrixFromRotationScaleTranslation(vec3 translation, vec4 rot, vec3 scale)
{
	mat3 scaleMatrix = mat3(
		vec3(scale.x, 0, 0),
		vec3(0, scale.y, 0),
		vec3(0, 0, scale.z)
	);

	mat3 rotMatrix = mat3_cast(rot);

	mat3 wsRotMulScale = scaleMatrix * rotMatrix;

	mat4 rot4x4 =
	{
		vec4(wsRotMulScale[0], 0),
		vec4(wsRotMulScale[1], 0),
		vec4(wsRotMulScale[2], 0),
		vec4(0, 0, 0, 1)
	}; 

	//rot4x4 = transpose(rot4x4);

	mat4 tr = transpose(mat4(
		vec4(1, 0, 0, 0),
		vec4(0, 1, 0, 0),
		vec4(0, 0, 1, 0),
		vec4(translation, 1)
	));

	return rot4x4 * tr;
}


void main() {
    // Combine static vertex position with per-instance offset
    vec2 offset = vertexPos.xy;
    mat4 splatToLocal = calcMatrixFromRotationScaleTranslation(gaussianPosition_ms.xyz, gaussianQuaternion, exp(gaussianPackedScale.xyz) * GAUSSIAN_CUTOFF_SCALE);
    
	mat4 splatToWorld = splatToLocal * u_objectToWorld;
	vec3 splatMeanWs = (vec4(gaussianPosition_ms.xyz, 1.0) * splatToWorld).xyz;

	float alphaScale;

	vec3 param  = computeConic(splatToWorld, transpose(u_worldToView), transpose(u_viewToClip), u_resolution, alphaScale);
	vec2 corner = computeCorner(offset, param, u_resolution);
	corner /= 2.0;
	vec4 splatClipPos = u_MVP * vec4(splatMeanWs, 1.0f);
	gl_Position = splatClipPos + vec4(corner * splatClipPos.w * GAUSSIAN_CUTOFF_SCALE, 0, 0);

	// Transform the vertex position to clip space
    //gl_Position = MVP * worldPosition;
    
    // Pass instance color to fragment shader
    out_color = gaussianColor.rgb;
	uv = offset;
}
