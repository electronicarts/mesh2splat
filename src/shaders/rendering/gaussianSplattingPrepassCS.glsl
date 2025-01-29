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
};

uniform float u_stdDev;
uniform vec3 u_hfov_focal;
uniform mat4 u_worldToView;
uniform mat4 u_viewToClip;

layout(std430, binding = 0) readonly buffer GaussianBuffer {
    GaussianVertex gaussians[];
} gaussianBuffer;

layout(std430, binding = 1) writeonly buffer PerQuadTransformations {
    QuadNdcTransformation ndcTransformations[];
} perQuadTransformations;


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

void computeCov3D(vec4 quat, vec3 scales, out mat3 sigma3d) {

	mat3 scaleMatrix = mat3(
		scales.x , 0, 0, 
		0, scales.y , 0,
		0, 0, scales.z 
	);

	mat3 rotMatrix;
	castQuatToMat3(quat, rotMatrix);

	mat3 mMatrix = scaleMatrix * rotMatrix;

	sigma3d = transpose(mMatrix) * mMatrix;
};

layout(local_size_x = 256) in;  
void main() {
	uint gid = gl_GlobalInvocationID.x;

    if (gid >= gaussianBuffer.gaussians.length()) return;

	GaussianVertex guassian = gaussianBuffer.gaussians[gid];
	
	mat3 cov3d;

	computeCov3D(guassian.rotation.wxyz, exp(guassian.scale.xyz), cov3d);
	vec4 gaussian_vs = u_worldToView * vec4(guassian.position.xyz, 1);

	vec4 pos2d = u_viewToClip * gaussian_vs;
	pos2d.xyz = pos2d.xyz / pos2d.w;
	pos2d.w = 1.f;

	vec2 wh = 2 * u_hfov_focal.xy * (u_hfov_focal.z);
	float limx = 1.3 * u_hfov_focal.x;
	float limy = 1.3 * u_hfov_focal.y;

	//projected screen-space coordinates
	float txtz = gaussian_vs.x / gaussian_vs.z;
	float tytz = gaussian_vs.y / gaussian_vs.z;

	float tx = min(limx, max(-limx, txtz)) * gaussian_vs.z;
	float ty = min(limy, max(-limy, tytz)) * gaussian_vs.z; 

	//Jacobian of affine approximation of projective transformation
	//We just care about how z affects, u_hfov_focal.xy is already baked into the projection mat
	mat3 J = mat3(
	  u_hfov_focal.z / gaussian_vs.z, 0., 0,
	  -(u_hfov_focal.x * tx) / (gaussian_vs.z * gaussian_vs.z), u_hfov_focal.z / gaussian_vs.z, 0,
	  0., -(u_hfov_focal.y * ty) / (gaussian_vs.z * gaussian_vs.z), 0.
	);
	
	mat3 T = transpose(mat3(u_worldToView)) * J;

	mat3 cov2d = transpose(T) * transpose(cov3d) * T;

	//Just care about applying low pass filter to 2x2 upper matrix
	cov2d[0][0] += 0.3f;
	cov2d[1][1] += 0.3f; 

	float det = determinant(mat2(cov2d));
	if (abs(det) < 1e-6) return;
	float det_inv = 1.f / det;

	vec2 quadwh_scr = vec2(3.f * sqrt(cov2d[0][0]), 3.f * sqrt(cov2d[1][1]));
	//from screen to NDC, inverse
	vec2 quadwh_ndc = (quadwh_scr / wh) * 2;

	perQuadTransformations.ndcTransformations[gid].gaussianMean2dNdc	= pos2d;
	perQuadTransformations.ndcTransformations[gid].quadScaleNdc			= vec4(quadwh_ndc, 0, 0);
	perQuadTransformations.ndcTransformations[gid].color				= guassian.color;

}

