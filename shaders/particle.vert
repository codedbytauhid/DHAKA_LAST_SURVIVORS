#version 330 core
layout (location = 0) in vec3 aPos;      // world position of this corner of the particle's quad
layout (location = 1) in vec2 aUV;       // 0..1 across the quad (used to draw a soft round dot)
layout (location = 2) in vec4 aColor;    // rgba (alpha already faded by the particle's age)
layout (location = 3) in float aGlow;    // 0 = normal blending, 1 = additive

uniform mat4 uView;
uniform mat4 uProjection;

out vec2 vUV;
out vec4 vColor;
out float vGlow;

void main()
{
    vUV = aUV;
    vColor = aColor;
    vGlow = aGlow;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
