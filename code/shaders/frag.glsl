#version 450
layout(location=0) out vec4 OutColor;
layout(location=0) in vec3 FragColor;

void main()
{
    OutColor = vec4(FragColor, 1.0);
}
