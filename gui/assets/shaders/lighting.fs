#version 330

// Blinn-Phong + rim light + fog exponentiel. La direction/couleur du soleil,
// l'ambiant et le fog arrivent en uniforms pilotés par EnvironmentState
// (cycle jour/nuit + saison + météo). Défauts posés côté C++ au chargement.

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 sunDir;       // direction de propagation de la lumière
uniform vec3 sunColor;     // HDR (peut dépasser 1)
uniform vec3 ambientColor;
uniform vec3 fogColor;
uniform float fogDensity;
uniform sampler2D shadowMap;
uniform mat4 lightVP;
uniform int shadowsOn;   // 0 tant qu'aucune passe n'a tourné
uniform int shadowMapRes;

out vec4 finalColor;

const float kSpecStrength = 0.22;
const float kShininess = 24.0;

// Ombre portée PCF 3x3 depuis la depth map de la lumière active.
float shadowFactor(vec3 worldPos, vec3 n, vec3 l)
{
    if (shadowsOn == 0)
        return 1.0;
    vec4 lp = lightVP * vec4(worldPos, 1.0);
    vec3 proj = lp.xyz / lp.w * 0.5 + 0.5;
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0 || proj.z > 1.0)
        return 1.0;
    float bias = max(0.0022 * (1.0 - dot(n, l)), 0.0006);
    float texel = 1.0 / float(shadowMapRes);
    float lit = 0.0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
        {
            float depth = texture(shadowMap, proj.xy + vec2(x, y) * texel).r;
            lit += (proj.z - bias <= depth) ? 1.0 : 0.0;
        }
    return 0.35 + 0.65 * (lit / 9.0); // ombre douce, jamais noire (l'ambiant vit)
}

void main()
{
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    if (texel.a < 0.05)
        discard;

    vec3 n = normalize(fragNormal);
    vec3 l = -normalize(sunDir);
    float diff = max(dot(n, l), 0.0);

    vec3 v = normalize(viewPos - fragPosition);
    vec3 h = normalize(l + v);
    float spec = pow(max(dot(n, h), 0.0), kShininess) * kSpecStrength * diff;

    // Rim : contre-jour doux pour détacher les silhouettes du ciel sombre.
    float rim = pow(1.0 - max(dot(n, v), 0.0), 3.0);
    float shadow = shadowFactor(fragPosition, n, l);
    vec3 lit = texel.rgb * (ambientColor + sunColor * diff * shadow)
             + sunColor * (spec * shadow + 0.18 * rim * diff)
             + ambientColor * 0.35 * rim;

    float dist = length(viewPos - fragPosition);
    float fogT = clamp(exp(-fogDensity * dist), 0.0, 1.0);
    finalColor = vec4(mix(fogColor, lit, fogT), texel.a);
}
