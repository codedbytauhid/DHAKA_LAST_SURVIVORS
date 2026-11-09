#version 330 core
in vec2 vUV;
in vec4 vColor;
in float vGlow;
out vec4 FragColor;

uniform float uBrightness;               // dark rooms: dust and blood should not glow brighter than the room

void main()
{
    vec2 p = vUV * 2.0 - 1.0;
    float d = dot(p, p);
    if (d > 1.0) discard;                // a round dot, not a square
    float soft = 1.0 - d;
    soft *= soft;                        // soft edge
    float a = vColor.a * soft;
    float light = mix(uBrightness, 1.0, vGlow);
    // premultiplied alpha: with glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA) a glowing particle (alpha output 0)
    // ADDS light, and a normal particle (smoke, blood, dust) is blended over the scene.
    FragColor = vec4(vColor.rgb * a * light * (1.0 + vGlow * 1.5), a * (1.0 - vGlow));
}
