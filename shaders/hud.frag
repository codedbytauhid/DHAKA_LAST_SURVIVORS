#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTex;
uniform int  uUseTex;
uniform vec4 uColor;                     // rgba (multiplies the texture when one is used)

void main()
{
    vec4 c = uColor;
    if (uUseTex == 1) c *= texture(uTex, vUV);
    FragColor = c;
}
