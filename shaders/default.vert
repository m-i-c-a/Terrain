#version 450 core
#extension GL_EXT_gpu_shader4 : enable

#define MORPH_TOP   1
#define MORPH_RIGHT 2
#define MORPH_BOT   4
#define MORPH_LEFT  8

layout (location = 0) in vec3 a_pos;

uniform mat4 u_projMatrix;
uniform mat4 u_viewMatrix;
uniform mat4 u_modelMatrix;

uniform float u_tileResolution;
uniform float u_scale;
uniform int  u_morphDirection;

uniform sampler2D u_heightMap;
uniform vec2 u_samplerDim;

out float v_height;
out vec2  v_uv;
out float v_blendFactor;

void main()
{
    vec3 worldPos = (u_modelMatrix * vec4(a_pos, 1.0f)).xyz;
    float myHeight = texelFetch(u_heightMap, ivec2(worldPos.x, worldPos.z), 0).x;
    vec4 neighborHeight = vec4(0.0f); // x = top , y = right, z = bot, w = left

    v_blendFactor = 0.0f;
    const float interpMinThreshold = 0.25f;
    const float interpMaxThreshold = 0.75f;

    if (bool(u_morphDirection & MORPH_TOP))
    {
        float pos = a_pos.z / u_tileResolution;
        if (pos >= interpMaxThreshold)
        {
            v_blendFactor = (pos - interpMaxThreshold) / (1.0f - interpMaxThreshold);
            neighborHeight.x = texelFetch(u_heightMap, ivec2(worldPos.x,u_tileResolution * (u_scale + 1)), 0).x;
        }
    }
    else if (bool(u_morphDirection & MORPH_BOT))
    {
        float pos = a_pos.z / u_tileResolution;
        if (pos <= interpMinThreshold)
        {
            v_blendFactor = 1.0f - (pos / interpMinThreshold);
        }
    }

    if (bool(u_morphDirection & MORPH_RIGHT))
    {
        float pos = a_pos.x / u_tileResolution;
        if (pos >= interpMaxThreshold)
        {
            v_blendFactor += (pos - interpMaxThreshold) / (1.0f - interpMaxThreshold); 
        }
    }
    else if (bool(u_morphDirection & MORPH_LEFT))
    {
        float pos = a_pos.x / u_tileResolution;
        if (pos <= interpMinThreshold)
        {
            v_blendFactor += (interpMinThreshold - pos) / interpMinThreshold;
        }
    }

    vec2 uv = vec2( worldPos.x / u_samplerDim.x, worldPos.z / u_samplerDim.y );
    float height = texelFetch(u_heightMap, ivec2(worldPos.x, worldPos.z), 0).x;
    worldPos.y += height * 50.0f;


    gl_Position = u_projMatrix * u_viewMatrix * vec4(worldPos, 1.0f);
    v_height = height;
    v_uv = uv;
}