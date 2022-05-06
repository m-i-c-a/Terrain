#version 450 core
// #extension GL_EXT_gpu_shader4 : enable

#define MORPH_TOP   1
#define MORPH_RIGHT 2
#define MORPH_BOT   4
#define MORPH_LEFT  8

layout (location = 0) in vec3 a_pos;

uniform mat4 u_projMatrix;
uniform mat4 u_viewMatrix;

uniform float u_tileResolution;
uniform int  u_morphDirection;

uniform vec4 u_tileInfo       ; // x=xOffset, y=zOffset, z=level, w=scale
uniform vec2 u_viewPos        ;
uniform sampler2D u_heightMap ;
uniform vec3 u_coarseOffset;

// Gui
uniform bool u_morphEnable = false;
uniform bool u_showMorphDebug = false;
uniform float u_heightScale = 1.0f;

out float v_height;
out float v_morphColor;

uniform float u_scale;
uniform vec2  u_translation;
uniform mat4  u_rotMatrix;
uniform vec2  u_shift;

void main()
{
    vec2 pos = u_translation + (u_rotMatrix * vec4(a_pos, 1.0f)).xz * u_scale;
    float height = texelFetch(u_heightMap, ivec2(pos.xy), 0).x;
    gl_Position = u_projMatrix * u_viewMatrix * vec4(pos.x, height * u_heightScale, pos.y, 1.0f);
    // gl_Position = u_projMatrix * u_viewMatrix * vec4(pos.x, pos.y, u_shift.x, 1.0f);

    v_height = height;
    v_morphColor = 0.0f;

    // vec2 snappedViewPos = floor(u_viewPos / u_tileInfo.w) * u_tileInfo.w;
    // ivec2 uv;
    // uv.x = int((a_pos.x * u_tileInfo.w) + snappedViewPos.x);
    // uv.y = int((a_pos.z * u_tileInfo.w) + snappedViewPos.y);

    // vec3 xyz = a_pos;
    // xyz.xz *= u_tileInfo.w;
    // xyz    += vec3(u_tileInfo.x + u_viewPos.x, 0.0f, u_tileInfo.y + u_viewPos.y);
    // // xyz    += vec3(u_tileInfo.x + uv.x, 0.0f, u_tileInfo.y + uv.y);

    // xyz.y  += texelFetch(u_heightMap, uv, 0).x;
    
    // v_morphColor *= float(u_showMorphDebug);
    // v_height = xyz.y;

    // xyz.y *= u_heightScale;
}