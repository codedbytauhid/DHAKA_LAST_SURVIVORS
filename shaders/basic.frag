#version 330 core
in vec3 vColor;
out vec4 FragColor;

uniform float uBrightness;

void main()
{
    FragColor = vec4(vColor * uBrightness, 1.0);
}