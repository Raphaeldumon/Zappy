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

out vec4 finalColor;

void main()
{
    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;

    // Saison : désature la texture vers la teinte du profil (neige, or...).
    float lum = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
    vec3 seasonal = mix(texel.rgb, groundOverlay * (0.35 + 0.9 * lum), groundMix);

    vec3 n = normalize(fragNormal);
    float diff = max(dot(n, -normalize(sunDir)), 0.0);
    vec3 lit = seasonal * (ambientColor + sunColor * diff);

    float dist = length(viewPos - fragPosition);
    float fogT = clamp(exp(-fogDensity * dist), 0.0, 1.0);
    finalColor = vec4(mix(fogColor, lit, fogT), texel.a);
}
