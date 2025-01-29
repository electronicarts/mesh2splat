#version 460 core

#define ALPHA_CUTOFF (1.0f/256.0f)
#define GAUSSIAN_CUTOFF_SCALE (sqrt(log(1.0f / ALPHA_CUTOFF)))

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
uniform vec2 u_resolution;
uniform vec3 u_camPos;

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

	GaussianVertex gaussian = gaussianBuffer.gaussians[gid];
	
	mat3 cov3d;
	float multiplier = gaussian.pbr.w == 1 ? u_stdDev : 1;

	computeCov3D(gaussian.rotation, exp(gaussian.scale.xyz) * GAUSSIAN_CUTOFF_SCALE * multiplier, cov3d);

	vec4 gaussian_vs = u_worldToView * vec4(gaussian.position.xyz, 1);

	vec4 pos2d = u_viewToClip * gaussian_vs;
	
	float clip = 1.2 * pos2d.w;

	if (pos2d.z < -clip || pos2d.x < -clip || pos2d.x > clip || pos2d.y < -clip || pos2d.y > clip) {
		perQuadTransformations.ndcTransformations[gid].gaussianMean2dNdc	= pos2d;
		perQuadTransformations.ndcTransformations[gid].quadScaleNdc			= vec4(0,0,0,0);
		perQuadTransformations.ndcTransformations[gid].color				= vec4(gaussian.color.rgb, 0);
		return;
    }


	pos2d.xyz = pos2d.xyz / pos2d.w;
	pos2d.w = 1.f;

	vec2 wh = 2 * u_hfov_focal.xy * (u_hfov_focal.z);
	float limx = 1.3 * u_hfov_focal.x;
	float limy = 1.3 * u_hfov_focal.y;


	vec3 t = mat3(u_worldToView) * (gaussian.position.xyz - u_camPos);

	//projected screen-space coordinates
	float txtz = t.x / t.z;
	float tytz = t.y / t.z;

	t.x = clamp(txtz, -limx, limx) * t.z;
    t.y = clamp(tytz, -limy, limy) * t.z;

	// Jacobian for the Taylor approximation of the nonlinear projective transformation
	//We just care about how z affects, u_hfov_focal.xy is already baked into the projection mat
	float tzSq = gaussian_vs.z * gaussian_vs.z;
	vec2 focal = vec2(u_viewToClip[0][0], u_viewToClip[1][1]);
    
	mat3 J_T = mat3(
        u_hfov_focal.z/gaussian_vs.z, 0., -u_hfov_focal.z*t.x/tzSq,
        0., u_hfov_focal.z/gaussian_vs.z , -u_hfov_focal.z*t.y/tzSq,
        0., 0., 0.
    );

	mat3 T = transpose(mat3(u_worldToView)) * J_T;

	mat3 cov2d = transpose(T) * cov3d * T;

    // covariance matrix in ray space

	//Just care about applying low pass filter to 2x2 upper matrix
	cov2d[0][0] += 0.3f;
	cov2d[1][1] += 0.3f; 

	float mid = 0.5*(cov2d[0][0] + cov2d[1][1]);
    float radius = length(vec2(0.5*(cov2d[0][0] - cov2d[1][1]), cov2d[0][1]));
    float lambda1 = mid + radius, lambda2 = mid - radius;
	if (lambda2 < 0.0) return;
    vec2 diagonalVector = normalize(vec2(cov2d[0][1], lambda1 - cov2d[0][0]));
    vec2 majorAxis = min(sqrt(3.0*lambda1), 1024.0) * diagonalVector;
    vec2 minorAxis = min(sqrt(3.0*lambda2), 1024.0) * vec2(diagonalVector.y, -diagonalVector.x);

	vec2 majorAxisMultiplier = majorAxis / u_resolution;
	vec2 minorAxisMultiplier = minorAxis / u_resolution;

	perQuadTransformations.ndcTransformations[gid].gaussianMean2dNdc	= pos2d;
	perQuadTransformations.ndcTransformations[gid].quadScaleNdc			= vec4(majorAxisMultiplier, minorAxisMultiplier);
	perQuadTransformations.ndcTransformations[gid].color				= gaussian.color;

}

