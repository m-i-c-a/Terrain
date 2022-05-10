#version 450 core

uniform bool u_showDebug;
uniform vec3 u_debugColor;

in float v_height;

out vec4 o_color;

void main(void)
{
	o_color = vec4(vec3(abs(v_height)), 1.0);
	o_color.rgb += (u_showDebug) ? u_debugColor : vec3(0.0f);
}