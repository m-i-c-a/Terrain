#version 450 core

uniform vec3 u_debugColor;

in float v_height;
in float v_morphColor;

out vec4 o_color;

void main(void)
{
	o_color = vec4(vec3(abs(v_height)), 1.0);
	o_color.r += v_morphColor;

	o_color = vec4(u_debugColor, 1.0f);
}