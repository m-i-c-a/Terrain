#version 450 core
// #extension GL_EXT_gpu_shader4 : enable

#define MORPH_TOP   1
#define MORPH_RIGHT 2
#define MORPH_BOT   4
#define MORPH_LEFT  8

layout (location = 0) in vec3 a_pos;

uniform mat4 u_projMatrix;
uniform mat4 u_viewMatrix;

uniform sampler2D u_heightMap;
uniform float u_heightScale = 1.0f;

out float v_height;
out float v_morphColor;

void main()
{
    vec3 xyz = a_pos;

    float myHeight = texelFetch(u_heightMap, ivec2(xyz.xz), 0).x;
    xyz.y = myHeight * u_heightScale - 0.01;

    gl_Position = u_projMatrix * u_viewMatrix * vec4(xyz, 1.0f);
    v_height = myHeight;
    v_morphColor = 0.0f;
}