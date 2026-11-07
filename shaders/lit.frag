#version 330 core
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform vec2  uTexScale;     // texture repeats (per world unit, or per face in mesh-UV mode)
uniform int   uUvMode;       // 0 = world-space UVs (walls, floors), 1 = the mesh's own UVs (signs, doors, windows)
uniform int   uMode;         // 0 = opaque, 1 = cut-out (discard transparent pixels), 2 = alpha blended (glass)
uniform vec3  uTint;
uniform float uShininess;
uniform float uSpecular;
uniform float uEmissive;     // self-lit amount (lamps, exit signs, screens)
uniform float uAlpha;

uniform vec3 uViewPos;

// Sun
uniform vec3  uSunDir;       // points TOWARD the sun
uniform vec3  uSunColor;
uniform float uSunScale;     // per object: 1 outdoors, small indoors (until shadow mapping exists)
uniform vec3  uAmbient;
uniform float uAmbientScale; // per object: indoor/corridor areas are darker

// Up to 8 point lights (the Level sends the 8 nearest to the camera)
const int MAX_LIGHTS = 8;
uniform int   uLightCount;
uniform vec3  uLightPos[MAX_LIGHTS];
uniform vec3  uLightColor[MAX_LIGHTS];
uniform float uLightRadius[MAX_LIGHTS];

// Fog
uniform vec3  uFogColor;
uniform float uFogDensity;

void main()
{
    vec3 N = normalize(vNormal);

    vec2 uv;
    if (uUvMode == 1) {
        uv = vUV;
    } else {
        // World-space UVs: pick the two world axes that face us, so bricks / tiles keep
        // the same size on every box regardless of how it was scaled.
        vec3 an = abs(N);
        if (an.y >= an.x && an.y >= an.z) uv = vWorldPos.xz;
        else if (an.x >= an.z)            uv = vWorldPos.zy;
        else                              uv = vWorldPos.xy;
    }
    uv *= uTexScale;

    vec4 texel = texture(uTexture, uv);
    float alpha = 1.0;
    if (uMode == 1) { if (texel.a < 0.5) discard; }
    else if (uMode == 2) { alpha = texel.a * uAlpha; }

    vec3 albedo = texel.rgb * uTint;
    vec3 V = normalize(uViewPos - vWorldPos);

    // ambient
    vec3 result = uAmbient * uAmbientScale * albedo;

    // sun (Blinn-Phong)
    vec3 L = normalize(uSunDir);
    float diff = max(dot(N, L), 0.0);
    vec3 H = normalize(L + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess) * uSpecular;
    result += (diff * albedo + spec) * uSunColor * uSunScale;

    // point lights
    for (int i = 0; i < uLightCount; ++i) {
        vec3 toLight = uLightPos[i] - vWorldPos;
        float dist = length(toLight);
        float atten = clamp(1.0 - dist / uLightRadius[i], 0.0, 1.0);
        atten *= atten;
        vec3 Lp = toLight / max(dist, 0.0001);
        float dP = max(dot(N, Lp), 0.0);
        vec3 Hp = normalize(Lp + V);
        float sP = pow(max(dot(N, Hp), 0.0), uShininess) * uSpecular;
        result += (dP * albedo + sP) * uLightColor[i] * atten;
    }

    // self-illumination
    result += albedo * uEmissive;

    // exponential-squared fog
    float d = length(uViewPos - vWorldPos);
    float fog = 1.0 - exp(-pow(d * uFogDensity, 2.0));
    result = mix(result, uFogColor, clamp(fog, 0.0, 1.0));

    FragColor = vec4(result, alpha);
}
