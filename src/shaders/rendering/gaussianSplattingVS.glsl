#version 460 core

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

uniform mat4 MVP;

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


//float4x4 calcMatrixFromRotationScaleTranslation(vec3 translation, vec4 rot, vec3 scale)
//{
//	float3x3 scaleMatrix = float3x3(
//		float3(scale.x, 0, 0),
//		float3(0, scale.y, 0),
//		float3(0, 0, scale.z));
//
//	float3x3 rotMatrix = matrixFromQuaternion(rot);
//
//	float3x3 wsRotMulScale = scaleMatrix * rotMatrix;
//
//	float4x4 rot4x4 =
//	{
//		float4(wsRotMulScale[0], 0),
//		float4(wsRotMulScale[1], 0),
//		float4(wsRotMulScale[2], 0),
//		float4(0, 0, 0, 1)
//	};
//
//	float4x4 tr = transpose(float4x4(
//		float4(1, 0, 0, 0),
//		float4(0, 1, 0, 0),
//		float4(0, 0, 1, 0),
//		float4(translation, 1)
//	));
//
//	return mul(tr, rot4x4);
//}


void main() {
    // Combine static vertex position with per-instance offset
    vec2 offset = vertexPos.xy;
    vec4 worldPosition = vec4(vertexPos.xyz * 0.1f + gaussianPosition_ms.xyz, 1.0);
    
    // Transform the vertex position to clip space
    gl_Position = MVP * worldPosition;
    
    // Pass instance color to fragment shader
    out_color = gaussianColor.rgb;
}
