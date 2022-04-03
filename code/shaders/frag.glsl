#version 450
layout(location=0) out vec4 OutColor;

layout(location=0) in vec3 FragColor;
layout(location=1) in vec2 FragTexCoord;

layout(binding =1) uniform sampler2D TexSampler;

void main()
{
    OutColor = texture(TexSampler,FragTexCoord );
}
