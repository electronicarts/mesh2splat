#version 460 core

// Static quad vertex position per-vertex
layout(location = 0) in vec3 vertexPos;

// Per-instance (quad) attributes
layout(location = 1) in vec4 gaussianMean2Ndc; 
layout(location = 2) in vec4 quadScaleNdc;
layout(location = 3) in vec4 color;
layout(location = 4) in vec4 conic;

uniform vec2 u_resolution;

out vec3 out_color;
out vec2 out_screen;
out float out_opacity;
out vec3 out_conic;

//

void main() {
	gl_Position = vec4(gaussianMean2Ndc.xy + (vertexPos.x * quadScaleNdc.xy + vertexPos.y * quadScaleNdc.zw) , 0, 1);
    out_conic = conic.xyz;
    out_color = color.rgb * color.a;
	out_opacity = color.a;
	vec2 ndc = gaussianMean2Ndc.xy ;
	out_screen = vec2((ndc + 1) * 0.5 * u_resolution);
}
