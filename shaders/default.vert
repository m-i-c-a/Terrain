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

// Gui
uniform float u_heightScale = 1.0f;

out float v_height;

uniform float u_scale;
uniform vec2  u_translation;
uniform mat4  u_rotMatrix;

void main()
{
    vec2 pos = u_translation + (u_rotMatrix * vec4(a_pos, 1.0f)).xz * u_scale;
    float height = texelFetch(u_heightMap, ivec2(pos.xy), 0).x;
    gl_Position = u_projMatrix * u_viewMatrix * vec4(pos.x, height * u_heightScale, pos.y, 1.0f);
    // gl_Position = u_projMatrix * u_viewMatrix * vec4(pos.x, 0.0f, pos.y, 1.0f);

    v_height = height;
}