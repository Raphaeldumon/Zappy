#version 330

// Éclairage du sol : même modèle que lighting.fs (sans spéculaire) + teinte
// saisonnière (groundOverlay/groundMix : herbe -> or -> roux -> neige).

in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 viewPos;
uniform vec3 sunDir;
uniform vec3 sunColor;
uniform vec3 ambientColor;
uniform vec3 fogColor;
uniform float fogDensity;
uniform vec3 groundOverlay;
uniform float groundMix;
uniform sampler2D shadowMap;
uniform mat4 lightVP;
uniform int shadowsOn;   // 0 tant qu'aucune passe n'a tourné
uniform int shadowMapRes;

out vec4 finalColor;

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

    // Saison : désature la texture vers la teinte du profil (neige, or...).
    float lum = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
    vec3 seasonal = mix(texel.rgb, groundOverlay * (0.35 + 0.9 * lum), groundMix);

    vec3 n = normalize(fragNormal);
    float diff = max(dot(n, -normalize(sunDir)), 0.0);
    float shadow = shadowFactor(fragPosition, n, -normalize(sunDir));
    vec3 lit = seasonal * (ambientColor + sunColor * diff * shadow);

    float dist = length(viewPos - fragPosition);
    float fogT = clamp(exp(-fogDensity * dist), 0.0, 1.0);
    finalColor = vec4(mix(fogColor, lit, fogT), texel.a);
}
