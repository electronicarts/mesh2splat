#version 430 core

in vec3 out_color;
in float out_opacity;
in vec2 out_uv;

out vec4 FragColor;

float splatAlpha(vec2 pos)
{
    float halcyonConstant = 7.55;

    float power = -0.5 * (dot(pos, pos)) * halcyonConstant;
    return clamp(exp(power), 0.0, 1.0);
}

void main() {			
    FragColor = vec4(out_color, out_opacity) * splatAlpha(out_uv);
}