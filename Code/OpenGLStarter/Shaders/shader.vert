#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform bool isDepthPass;

void main()
{
    if (isDepthPass)
    {
        gl_Position = lightSpaceMatrix * model * vec4(pos, 1.0);
    }
    else
    {
        gl_Position = projection * view * model * vec4(pos, 1.0);
        FragPos = vec3(model * vec4(pos, 1.0));
        FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
        TexCoord = aTexCoord;
        Normal = mat3(transpose(inverse(model))) * aNormal;
    }
}