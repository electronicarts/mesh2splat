#version 460 core

#define ALPHA_CUTOFF (1.0f/256.0f)
#define GAUSSIAN_CUTOFF_SCALE (sqrt(log(1.0f / ALPHA_CUTOFF)))
#define MIN_PIXEL_RADIUS 1.0f

// Static quad vertex position per-vertex
layout(location = 0) in vec3 vertexPos;

// Per-instance (quad) attributes
layout(location = 1) in vec4 gaussianPosition_ms; //model space  
layout(location = 2) in vec4 gaussianColor;
layout(location = 3) in vec4 gaussianPackedScale;
layout(location = 4) in vec4 gaussianNormal;
layout(location = 5) in vec4 gaussianQuaternion;
layout(location = 6) in vec4 gaussianPbr;

// You can add more per-instance attributes at locations 3, 4, 5 as needed.
out vec3 out_color;
out vec2 out_uv;
out float out_opacity;
out vec3 out_conic;
out vec2 out_coordxy;

uniform mat4 u_MVP;
uniform mat4 u_worldToView;	
uniform mat4 u_viewToClip;	
uniform mat4 u_objectToWorld; 
uniform vec2 u_resolution;
uniform vec3 u_hfov_focal;

//TODO: Should define some common imports, this func should be used also in other shaders
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

	// sigma3D = R*S*S.T*R.T 
	sigma3d = transpose(mMatrix) * mMatrix;
};

void main() {

	mat3 cov3d;

	//vec3 gaussianWorld = u_objectToWorld * vec4(gaussianPosition_ms.xyz, 1);
	//TODO: I am technically having to log and then exp the same scale values along the pipeline, but whatever
	computeCov3D(gaussianQuaternion, vec3(exp(gaussianPackedScale.xy), max(exp(gaussianPackedScale.x), exp(gaussianPackedScale.y))), cov3d);
	vec4 gaussian_vs = u_worldToView * vec4(gaussianPosition_ms.xyz, 1);

	vec4 pos2dHom = u_viewToClip * gaussian_vs;
	
	//persective divide
	pos2dHom.xyz = pos2dHom.xyz / pos2dHom.w;
	pos2dHom.w = 1.f;

	vec2 wh = 2 * u_hfov_focal.xy * u_hfov_focal.z;
	float limx = 1.3 * u_hfov_focal.x;
	float limy = 1.3 * u_hfov_focal.y;

	//To NDC with persp div
	float txtz = gaussian_vs.x / gaussian_vs.z;
	float tytz = gaussian_vs.y / gaussian_vs.z;

	float tx = min(limx, max(-limx, txtz)) * gaussian_vs.z;
	float ty = min(limy, max(-limy, tytz)) * gaussian_vs.z; 

	if (any(greaterThan(abs(pos2dHom.xyz), vec3(1.3)))) {
		gl_Position = vec4(-100, -100, -100, 1);
		return;	
	}

	//Jacobian of affine approximation of projective transformation
	mat3 J = mat3(
	  u_hfov_focal.z / gaussian_vs.z, 0., 0,
	  -(u_hfov_focal.z * tx) / (gaussian_vs.z * gaussian_vs.z), u_hfov_focal.z / gaussian_vs.z, 0,
	  0., -(u_hfov_focal.z * ty) / (gaussian_vs.z * gaussian_vs.z), 0.
	);
	
	mat3 JW = J * mat3(u_worldToView);
	mat3 cov2d = JW * cov3d * transpose(JW);

	//Just care about applying low pass filter to 2x2 upper matrix
	cov2d[0][0] += 0.3f;
	cov2d[1][1] += 0.3f; 

	float det = determinant(mat2(cov2d));
	if (det == 0.0f){
		gl_Position = vec4(10000.f, 10000.f, 10000.f, 10000.f);
		return;
	}
	float det_inv = 1.f / det;


	vec2 quadwh_scr = vec2(3.f * sqrt(cov2d[0][0]), 3.f * sqrt(cov2d[1][1]));
	vec2 quadwh_ndc = quadwh_scr / wh * 2; //HMM
	pos2dHom.xy = pos2dHom.xy + vertexPos.xy * quadwh_ndc;

	gl_Position = pos2dHom;
	//save space using vec3 as its a 2x2 symmetric
	//Should actually use the conic to compute varying density
	out_conic = vec3(cov2d[1][1] * det_inv, -cov2d[0][1] * det_inv, cov2d[0][0] * det_inv);
	out_color = gaussianColor.rgb;
	out_opacity = gaussianColor.a;
	out_uv = vertexPos.xy;

}
