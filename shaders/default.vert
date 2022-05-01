#version 450 core

layout (location = 0) in vec3 a_pos;

uniform mat4 u_projMatrix;
uniform mat4 u_viewMatrix;

uniform vec4 u_offset;
uniform vec4 u_scale;

// layout (location = 1) in vec3 a_norm;
// // layout (location = 2) in vec2 a_texcoord;

// uniform mat4 u_model_mat;

void main()
{
	vec4 pos = u_scale*(u_offset+vec4(a_pos,1.0));

    gl_Position = u_projMatrix * u_viewMatrix * pos;

    // gl_Position = u_proj_mat * u_view_mat * u_model_mat * vec4(a_pos, 1.0f);
    // gl_Position = vec4(a_pos, 1.0f);
}