#version 460 core
layout(triangles, equal_spacing, cw) in;

// Updated input to include all vertex attributes, with tangent as vec4
in TCS_OUT {
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
    vec2 normalizedUv;
    vec3 scale;
} tes_in[];

// Updated output to include all vertex attributes, with tangent as vec4
out TES_OUT {
    vec3 position;
    vec3 normal;
    vec4 tangent;
    vec2 uv;
    vec2 normalizedUv;
    vec3 scale;
} tes_out;

void main() {
    // Barycentric coordinates for the tessellated vertex
    float u = gl_TessCoord.x;
    float v = gl_TessCoord.y;
    float w = gl_TessCoord.z;

    // Interpolate position using barycentric coordinates
    vec3 p0 = u * tes_in[0].position;
    vec3 p1 = v * tes_in[1].position;
    vec3 p2 = w * tes_in[2].position;
    tes_out.position = p0 + p1 + p2;

    // Interpolate normal
    tes_out.normal = normalize(u * tes_in[0].normal + v * tes_in[1].normal + w * tes_in[2].normal);

    // Interpolate tangent with vec4
    tes_out.tangent = normalize(vec4(
        u * tes_in[0].tangent.xyz + v * tes_in[1].tangent.xyz + w * tes_in[2].tangent.xyz,
        sign(u * tes_in[0].tangent.w + v * tes_in[1].tangent.w + w * tes_in[2].tangent.w)
    ));

    // Interpolate UV
    tes_out.uv = u * tes_in[0].uv + v * tes_in[1].uv + w * tes_in[2].uv;
    tes_out.normalizedUv = u * tes_in[0].normalizedUv + v * tes_in[1].normalizedUv + w * tes_in[2].normalizedUv;

    tes_out.scale = tes_in[0].scale;

    // Set gl_Position for the vertex
    gl_Position = vec4(tes_out.position, 1.0);
}
