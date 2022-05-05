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

void main()
{
    v_morphColor = 0.0f;

    vec2 snappedViewPos = floor(u_viewPos / u_tileInfo.w) * u_tileInfo.w;
    ivec2 uv;
    uv.x = int((a_pos.x * u_tileInfo.w) + snappedViewPos.x);
    uv.y = int((a_pos.z * u_tileInfo.w) + snappedViewPos.y);

    vec3 xyz = a_pos;
    xyz.xz *= u_tileInfo.w;
    xyz    += vec3(u_tileInfo.x + u_viewPos.x, 0.0f, u_tileInfo.y + u_viewPos.y);
    // xyz    += vec3(u_tileInfo.x + uv.x, 0.0f, u_tileInfo.y + uv.y);

    xyz.y  += texelFetch(u_heightMap, uv, 0).x;

    // if (u_morphEnable && ((mod(a_pos.x, 2) != 0) || (mod(a_pos.z, 2) != 0)))
    // {
    //     const float morphScale = 0.25f;
    //     const float morphLength = morphScale * u_tileResolution; 
    //     const float maxMorphStart = u_tileResolution - morphLength; 

    //     if (bool(u_morphDirection & MORPH_TOP))
    //     {
    //         if (a_pos.z == u_tileResolution)
    //         {
    //             vec3 coarsePos = (a_pos * (u_tileInfo.w + 1)) + u_coarseOffset;
    //             float rHeight = texelFetch(u_heightMap, ivec2(coarsePos.x, coarsePos.z), 0).x;
    //             // float lHeight = texelFetch(u_heightMap, ivec2(xyz.x - u_tileInfo.w, xyz.z), 0).x;
    //             // xyz.y = (rHeight + lHeight) / 2.0f;
    //             xyz.y = rHeight;
    //             v_morphColor = 1.0f;
    //         }
    //         // if (a_pos.z >= maxMorphStart)
    //         // {
    //         //     float rHeight = texelFetch(u_heightMap, ivec2(xyz.x + u_tileInfo.w, xyz.z), 0).x;
    //         //     float lHeight = texelFetch(u_heightMap, ivec2(xyz.x - u_tileInfo.w, xyz.z), 0).x;
    //         //     float weight = (a_pos.z - maxMorphStart) / morphLength;
    //         //     // xyz.y = mix(xyz.y, (rHeight + lHeight) / 2.0f, weight);
    //         //     xyz.y = (rHeight + lHeight) / 2.0f;
    //         //     v_morphColor = 1.0f;
    //         // }
    //     }
    //     else if (bool(u_morphDirection & MORPH_BOT))
    //     {
    //         if (a_pos.z == 0)
    //         {
    //             float rHeight = texelFetch(u_heightMap, ivec2(xyz.x + u_tileInfo.w, xyz.z), 0).x;
    //             float lHeight = texelFetch(u_heightMap, ivec2(xyz.x - u_tileInfo.w, xyz.z), 0).x;
    //             xyz.y = (rHeight + lHeight) / 2.0f;
    //             v_morphColor = 1.0f;
    //         }
    //     }

    //     if (bool(u_morphDirection & MORPH_RIGHT))
    //     {
    //         if (a_pos.x == u_tileResolution)
    //         {
    //             float tHeight = texelFetch(u_heightMap, ivec2(xyz.x, xyz.z + u_tileInfo.w), 0).x;
    //             float bHeight = texelFetch(u_heightMap, ivec2(xyz.x, xyz.z - u_tileInfo.w), 0).x;
    //             xyz.y = (tHeight + bHeight) / 2.0f;
    //             v_morphColor = 1.0f;
    //         }
    //     }
    //     else if (bool(u_morphDirection & MORPH_LEFT))
    //     {
    //         if (a_pos.x == 0)
    //         {
    //             float tHeight = texelFetch(u_heightMap, ivec2(xyz.x, xyz.z + u_tileInfo.w), 0).x;
    //             float bHeight = texelFetch(u_heightMap, ivec2(xyz.x, xyz.z - u_tileInfo.w), 0).x;
    //             xyz.y = (tHeight + bHeight) / 2.0f;
    //             v_morphColor = 1.0f;
    //         }
    //     }
    // }
    
    v_morphColor *= float(u_showMorphDebug);
    v_height = xyz.y;

    xyz.y *= u_heightScale;
    gl_Position = u_projMatrix * u_viewMatrix * vec4(xyz, 1.0f);
}