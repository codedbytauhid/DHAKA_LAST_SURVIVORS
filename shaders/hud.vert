#version 330 core
layout (location = 0) in vec3 aPos;      // the quad's corners, 0..1
layout (location = 2) in vec2 aUV;

uniform vec4 uRect;                      // x, y of the bottom-left corner and width, height, in normalized device coords
uniform vec4 uUV;                        // part of the texture to show: u, v of the corner and width, height (text uses one glyph cell)

out vec2 vUV;

void main()
{
    vUV = uUV.xy + aUV * uUV.zw;
    gl_Position = vec4(uRect.xy + aPos.xy * uRect.zw, 0.0, 1.0);
}
