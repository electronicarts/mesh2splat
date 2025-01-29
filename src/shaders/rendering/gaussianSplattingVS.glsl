#version 460 core

// Static quad vertex position per-vertex
layout(location = 0) in vec3 vertexPos;

// Per-instance (quad) attributes
layout(location = 1) in vec4 gaussianMean2Ndc; 
layout(location = 2) in vec4 quadScaleNdc;
layout(location = 3) in vec4 color;

out vec3 out_color;
out vec2 out_uv;
out float out_opacity;

void main() {
	vec4 pos2dNdc = gaussianMean2Ndc;

	pos2dNdc.xy = pos2dNdc.xy + vertexPos.xy * quadScaleNdc.xy;
	//pos2dNdc.w = 1;

	gl_Position = vec4(gaussianMean2Ndc.xy + (vertexPos.x*quadScaleNdc.xy + vertexPos.y*quadScaleNdc.zw), 0, 1);
	//TODO: Should actually use the conic to compute varying density
	//out_conic = vec3(cov2d[1][1] * det_inv, -cov2d[0][1] * det_inv, cov2d[0][0] * det_inv);
	out_color = color.rgb * color.a;
	out_opacity = color.a;
	out_uv = vertexPos.xy;
}
