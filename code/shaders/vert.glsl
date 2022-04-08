#version 450

layout (location=0) in vec3 InPosition;
layout (location =1) in vec3 InColor;
layout (location =2) in vec2 InTexCoord;

layout (location = 0) out vec3 FragColor;
layout (location = 1) out vec2 FragTexCoord;

layout (binding = 0) uniform UniformBufferObject
{
    mat4 Model;
    mat4 View;
    mat4 Proj;
} Ubo;

void main()
{
    vec4 tmp=Ubo.Proj*Ubo.View*Ubo.Model*vec4(InPosition, 1.0);
    //tmp.zw=vec2(0.0,5.0);
    gl_Position = tmp;
    FragColor = InColor;
    FragTexCoord= InTexCoord;
}
