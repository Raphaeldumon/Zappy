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

out vec4 finalColor;

const float kSpecStrength = 0.22;
const float kShininess = 24.0;

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
    vec3 lit = texel.rgb * (ambientColor + sunColor * diff)
             + sunColor * (spec + 0.18 * rim * diff)
             + ambientColor * 0.35 * rim;

    float dist = length(viewPos - fragPosition);
    float fogT = clamp(exp(-fogDensity * dist), 0.0, 1.0);
    finalColor = vec4(mix(fogColor, lit, fogT), texel.a);
}
