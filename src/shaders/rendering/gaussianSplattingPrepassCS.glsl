///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: fast mesh to 3D gaussian splat conversion             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#version 460 core

struct GaussianVertex {
    vec4 position;
    vec4 color;
    vec4 scale;
    vec4 normal;
    vec4 rotation;
    vec4 pbr;
};

struct QuadNdcTransformation {
    vec4 gaussianMean2dNdc;
    vec4 quadScaleNdc;
    vec4 color;
	vec4 conic;
	vec4 normal;
	vec4 wsPos;
};

uniform sampler2D u_depthTexture;

uniform float u_stdDev;
uniform mat4 u_worldToView;
uniform mat4 u_viewToClip;
uniform mat4 u_modelToWorld;
uniform vec2 u_resolution;
uniform vec2 u_nearFar;
uniform uint u_depthTestMesh;

uniform int u_renderMode;
uniform uint u_format;
uniform int u_gaussianCount;

layout(std430, binding = 0) readonly buffer GaussianBuffer {
    GaussianVertex gaussians[];
} gaussianBuffer;

layout(std430, binding = 1) writeonly buffer GaussianDepthPostFiltering {
    float depths_vs[];
} gaussianDepthPostFiltering;

layout(std430, binding = 2) writeonly buffer PerQuadTransformations {
    QuadNdcTransformation ndcTransformations[];
} perQuadTransformations;

layout(binding = 3) uniform atomic_uint g_validCounter;

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
};

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


layout(local_size_x = 16, local_size_y = 16) in;  
void main() {
	uint globalWidth = gl_NumWorkGroups.x * gl_WorkGroupSize.x;
    uint gid = gl_GlobalInvocationID.y * globalWidth + gl_GlobalInvocationID.x;

    if (gid >= u_gaussianCount) return;

	GaussianVertex gaussian = gaussianBuffer.gaussians[gid];
	
	vec4 gaussianWs =  u_modelToWorld * vec4(gaussian.position.xyz, 1);

	vec4 gaussian_vs = u_worldToView * vec4(gaussianWs.xyz, 1);

	vec4 pos2d = u_viewToClip * gaussian_vs;

	float clip = 1.05 * pos2d.w; //Need to see how conservative that is

	if (pos2d.z < -clip || pos2d.x < -clip || pos2d.x > clip || pos2d.y < -clip || pos2d.y > clip) {
		return;
    }

	//Not working for transparent ones yet
	bool isDepthTestEnabled = u_depthTestMesh == 1 ? true : false;

	if (isDepthTestEnabled && gaussian.color.a > .95f && u_format == 0 )
	{
		vec2 ndc = (pos2d.xy / pos2d.w);      
		vec2 uv  = ndc * 0.5 + 0.5;         
		float depth = texture(u_depthTexture, uv).r;
		float myDepth = (pos2d.z / pos2d.w) * 0.5 + 0.5;
		float eps = 0.00002f;
		if (myDepth > depth + eps) {
			return;
		}
	}

	float multiplier = (u_format == 0 || u_format == 3)  ? u_stdDev : 1.0;
	vec3 modelScale = vec3(length(u_modelToWorld[0]), length(u_modelToWorld[0]), length(u_modelToWorld[1]));
	vec3 scale = gaussian.scale.xyz * multiplier * (modelScale * modelScale);

	mat3 cov3d;
	mat3 rotMatrix;
	castQuatToMat3(gaussian.rotation, rotMatrix);

	mat3 modelRotation = mat3(
			u_modelToWorld[0].xyz,
			u_modelToWorld[1].xyz,
			u_modelToWorld[2].xyz
		);
    
	rotMatrix = rotMatrix * inverse(modelRotation);
    
	computeCov3D(rotMatrix, scale, cov3d);

	vec4 outputColor = vec4(0, 0, 0, 0);
	vec4 computedNormal_Ws = vec4(1, 0, 0, 0);

	float normalizedDepth = (-(gaussian_vs.z) - u_nearFar.x) / (u_nearFar.y - u_nearFar.x); 
	float invertedLinearizedDepth = clamp(normalizedDepth, 0, 1);
	float expDepthFallof = exp(-20.0f * invertedLinearizedDepth);
	float computedDepth = clamp((expDepthFallof), 0, 1);
	

	if (u_format == 0 || u_format == 3)
	{
		vec3 normalWs = (transpose(inverse(u_modelToWorld)) * vec4(gaussian.normal.xyz, 1.0f)).xyz;
		computedNormal_Ws = vec4((normalWs * .5) + .5, gaussian.color.a); //remember to decode this when using in the gbuffer
	}

	else if (u_format == 1)
	{
		//Shortest axis direction normal observation made at page 4 of https://arxiv.org/pdf/2311.17977
		//Cool trick for min index: https://computergraphics.stackexchange.com/questions/13662/glsl-get-min-max-index-of-vec3
		uint minCompIndex = uint((gaussian.scale.y < gaussian.scale.z) && (gaussian.scale.y < gaussian.scale.x)) + (uint((gaussian.scale.z < gaussian.scale.y) && (gaussian.scale.z < gaussian.scale.x)) * 2);
		vec3 rgbN = ((rotMatrix[minCompIndex].xyz * .5) + .5);
		computedNormal_Ws = vec4(rgbN, gaussian.color.a);  
	}

	if (u_renderMode == 0)
	{
		outputColor = gaussian.color;

	}
	else if (u_renderMode == 1) 
	{
		outputColor = vec4(vec3(computedDepth), gaussian.color.a);
	}
	else if (u_renderMode == 2)
	{
		outputColor = computedNormal_Ws;
	}
	if (u_renderMode == 3)
	{
		outputColor = vec4(random2d(gl_GlobalInvocationID.xy), random2d(gl_GlobalInvocationID.yx), random2d(gl_GlobalInvocationID.yx * 1.234), 1.0);
	}
	
	pos2d.xyz = pos2d.xyz / pos2d.w;

	//https://github.com/graphdeco-inria/diff-gaussian-rasterization/blob/59f5f77e3ddbac3ed9db93ec2cfe99ed6c5d121d/cuda_rasterizer/forward.cu#L74 
    float tzSq = gaussian_vs.z * gaussian_vs.z;
    float jsx = -(u_viewToClip[0][0] * u_resolution.x) / (2 * gaussian_vs.z);
    float jsy = -(u_viewToClip[1][1] * u_resolution.y) / (2 * gaussian_vs.z);
    float jtx = (u_viewToClip[0][0] * gaussian_vs.x * u_resolution.x) / (2 * tzSq);
    float jty = (u_viewToClip[1][1] * gaussian_vs.y * u_resolution.y) / (2 * tzSq);
    float jtz = ((u_nearFar.y - u_nearFar.x) * u_viewToClip[3][2]) / (2*tzSq);

    mat3 J = mat3(vec3(jsx, 0.0f, 0.0f),
                  vec3(0.0f, jsy, 0.0f),
                  vec3(jtx, jty, jtz));

    mat3 W = mat3(u_worldToView);
    mat3 JW = J * W;

    mat3 V_prime = JW * cov3d * transpose(JW);

    mat2 cov2d = mat2(V_prime);

	//Just care about applying low pass filter to 2x2 upper matrix
	cov2d[0][0] += 0.3f;
	cov2d[1][1] += 0.3f; 

	float mid = (cov2d[0][0] + cov2d[1][1]);
    
	float delta = length(vec2((cov2d[0][0] - cov2d[1][1]), (2 * cov2d[0][1])));
	
	float lambda1 = 0.5 * (mid + delta);
	float lambda2 = 0.5 * (mid - delta);

	if (lambda2 < 0.0) return;

    vec2 diagonalVector = normalize(vec2(1, (-cov2d[0][0]+cov2d[0][1]+lambda1)/(cov2d[0][1]-cov2d[1][1]+lambda1)));
	vec2 majorAxis = min(3 * sqrt(lambda1), 1024.0) * diagonalVector;
    vec2 minorAxis = min(3 * sqrt(lambda2), 1024.0) * vec2(diagonalVector.y, -diagonalVector.x);

	vec2 majorAxisMultiplier =  majorAxis / (u_resolution * 0.5);
	vec2 minorAxisMultiplier =  minorAxis / (u_resolution * 0.5);

	uint gaussianIndex = atomicCounterIncrement(g_validCounter);

	perQuadTransformations.ndcTransformations[gaussianIndex].gaussianMean2dNdc	= pos2d;
	perQuadTransformations.ndcTransformations[gaussianIndex].quadScaleNdc		= vec4(majorAxisMultiplier, minorAxisMultiplier);
	perQuadTransformations.ndcTransformations[gaussianIndex].color				= outputColor;

	mat2 conic = inverseMat2(cov2d);
	perQuadTransformations.ndcTransformations[gaussianIndex].conic				= vec4(conic[0][0], conic[0][1], conic[1][1], -gaussian_vs.z ); //I embed the depth here, easier (for now) and cheaper

	perQuadTransformations.ndcTransformations[gaussianIndex].normal				= vec4(computedNormal_Ws.xyz, gaussian.pbr.x);
	perQuadTransformations.ndcTransformations[gaussianIndex].wsPos				= vec4(gaussianWs.xyz, gaussian.pbr.y);

	gaussianDepthPostFiltering.depths_vs[gaussianIndex]							= gaussian_vs.z;
}

