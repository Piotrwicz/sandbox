#version 330

uniform vec3 u_eye;
uniform mat4 u_viewProj;
uniform mat4 u_modelMatrix;
uniform mat4 u_modelMatrixIT;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inVertexColor;
layout(location = 3) in vec2 inTexcoord;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

out vec3 v_world_position;
out vec3 v_normal;
out vec2 v_texcoord;
out vec3 v_eyeDir;

void main()
{
    vec4 worldPos = u_modelMatrix * vec4(inPosition, 1);
    v_world_position = worldPos.xyz;
    v_normal = normalize((u_modelMatrixIT * vec4(inNormal,0)).xyz);
    v_texcoord = inTexcoord;
    v_eyeDir = normalize(u_eye - worldPos.xyz);
    gl_Position = u_viewProj * worldPos;
}