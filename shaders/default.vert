#version 450 core

layout (location = 0) in vec3 a_pos;

uniform mat4 u_projMatrix;
uniform mat4 u_viewMatrix;
uniform mat4 u_modelMatrix;

uniform sampler2D u_heightMap;
uniform vec2 u_samplerDim;

out float v_height;
out vec2  v_uv;

void main()
{
    vec3 worldPos = (u_modelMatrix * vec4(a_pos, 1.0f)).xyz;
    vec2 uv = vec2( worldPos.x / u_samplerDim.x, worldPos.z / u_samplerDim.y );
    float height = texelFetch(u_heightMap, ivec2(worldPos.x, worldPos.z), 0).x;
    worldPos.y += height * 50.0f;


    gl_Position = u_projMatrix * u_viewMatrix * vec4(worldPos, 1.0f);
    v_height = height;
    v_uv = uv;
}