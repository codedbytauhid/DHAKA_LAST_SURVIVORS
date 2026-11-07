#version 330 core
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;

void main()
{
    // Debug look: color each face by the direction it points.
    // (Real textures and lighting come in the next steps.)
    vec3 n = normalize(vNormal);
    FragColor = vec4(n * 0.5 + 0.5, 1.0);
}