#version 450 core

in float v_height;
in vec2  v_uv;
in float v_blendFactor;

out vec4 o_color;

void main(void)
{
	// o_color = vec4(v_uv, 0.0, 1.0); // texture2D(tex_terrain,vs_tex_coord);
	o_color = vec4(vec3(abs(v_height)), 1.0); // texture2D(tex_terrain,vs_tex_coord);
	o_color.r += v_blendFactor;
}