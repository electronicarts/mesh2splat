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
};

uniform float u_stdDev;
uniform vec3 u_hfov_focal;
uniform mat4 u_worldToView;
uniform mat4 u_viewToClip;
uniform vec2 u_resolution;
uniform vec2 u_nearFar;

uniform vec3 u_camPos;
uniform int u_renderMode;
uniform unsigned int u_format;
uniform int u_gaussianCount;

layout(std430, binding = 0) readonly buffer GaussianBuffer {
    GaussianVertex gaussians[];
} gaussianBuffer;

layout(std430, binding = 1) writeonly buffer GaussianBufferOutPostFilter {
    GaussianVertex gaussians[];
} gaussianBufferOutPostFilter;

layout(std430, binding = 2) writeonly buffer PerQuadTransformations {
    QuadNdcTransformation ndcTransformations[];
} perQuadTransformations;


layout(binding = 3) uniform atomic_uint g_validCounter;

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

layout(local_size_x = 256) in;  
void main() {
	uint gid = gl_GlobalInvocationID.x;

    if (gid >= u_gaussianCount) return;

	GaussianVertex gaussian = gaussianBuffer.gaussians[gid];
	
	vec4 gaussian_vs = u_worldToView * vec4(gaussian.position.xyz, 1);

	vec4 pos2d = u_viewToClip * gaussian_vs;
	
	float clip = 1.05 * pos2d.w; //Need to see how conservative that is

	//TODO: quite conservative honestly
	if (pos2d.z < -clip || pos2d.x < -clip || pos2d.x > clip || pos2d.y < -clip || pos2d.y > clip) {
		return;
    }

	float multiplier = u_format == 0 ? u_stdDev : 1.0;
	vec3 scale = gaussian.scale.xyz * multiplier ;

	mat3 cov3d;
	mat3 rotMatrix;
	castQuatToMat3(gaussian.rotation, rotMatrix);
	computeCov3D(rotMatrix, scale, cov3d);
	//TODO: probably better with shader permutation (?)
	vec4 outputColor = vec4(0, 0, 0, 0);
	if (u_renderMode == 0)
	{
		outputColor = gaussian.color;

	}
	else if (u_renderMode == 1) //Depth
	{
		float normalizedDepth = (-(gaussian_vs.z) - 0.01) / (100.0 - 0.01); //Hardcoded znear and zfar, 
		float invertedLinearizedDepth = clamp(normalizedDepth, 0, 1);
		float expDepthFallof = exp(-10 * invertedLinearizedDepth);
		float zSq = clamp((expDepthFallof), 0, 1);
		outputColor = vec4(zSq, zSq, zSq, gaussian.color.a);
	}
	else if (u_renderMode == 2) //Normal
	{
		if (u_format == 0)
		{
			outputColor = vec4((gaussian.normal.xyz / 2.0) + 0.5, gaussian.color.a);
		}
		else if (u_format == 1)
		{
			outputColor = vec4((rotMatrix[2] / 2.0) + 0.5, gaussian.color.a);
		}
	}
	
	pos2d.xyz = pos2d.xyz / pos2d.w;

    float tzSq = gaussian_vs.z * gaussian_vs.z;
    float jsx = -(u_viewToClip[0][0] * u_resolution.x) / (2.0f * gaussian_vs.z);
    float jsy = -(u_viewToClip[1][1] * u_resolution.y) / (2.0f * gaussian_vs.z);
    float jtx = (u_viewToClip[0][0] * gaussian_vs.x * u_resolution.x) / (2.0f * tzSq);
    float jty = (u_viewToClip[1][1] * gaussian_vs.y * u_resolution.y) / (2.0f * tzSq);
    float jtz = ((100 - 0.01) * u_viewToClip[3][2]) / (2.0f * tzSq);

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
    //I am not sure why but I need to do 6* rather than the classic 3* std dev. Something may be wrong in how I compute the lambdas
	vec2 majorAxis = min(6 * sqrt(lambda1), 1024.0) * diagonalVector;
    vec2 minorAxis = min(6 * sqrt(lambda2), 1024.0) * vec2(diagonalVector.y, -diagonalVector.x);

	vec2 majorAxisMultiplier = majorAxis / u_resolution;
	vec2 minorAxisMultiplier = minorAxis / u_resolution;

	uint gaussianIndex = atomicCounterIncrement(g_validCounter);

	perQuadTransformations.ndcTransformations[gaussianIndex].gaussianMean2dNdc	= pos2d;
	perQuadTransformations.ndcTransformations[gaussianIndex].quadScaleNdc		= vec4(majorAxisMultiplier, minorAxisMultiplier);
	perQuadTransformations.ndcTransformations[gaussianIndex].color				= outputColor;

	mat2 conic = inverseMat2(cov2d);
	perQuadTransformations.ndcTransformations[gaussianIndex].conic				= vec4(conic[0][0], conic[0][1], conic[1][1], 1.0);


	//TODO: I would just need the view space depth here tbh, not the whole gaussian. This would probably make it faster
	gaussianBufferOutPostFilter.gaussians[gaussianIndex] = gaussian;
}

