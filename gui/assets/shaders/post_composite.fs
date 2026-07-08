#version 330

// Composite final : scène HDR + bloom + god rays + ACES + grading + vignette
// + grain + distorsion de chaleur. Remplace bloom_combine.fs dans la chaîne.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;  // scène HDR pleine résolution (RGBA16F)
uniform sampler2D glowTex;   // bloom demi-résolution
uniform vec2 sunScreen;      // position écran du soleil, UV 0..1
uniform float godray;        // force des god rays (0 = off / soleil hors champ)
uniform vec3 gradeLift;
uniform vec3 gradeGain;
uniform float heat;          // 0..1 distorsion de canicule
uniform float time;

out vec4 finalColor;

const float kBloomStrength = 0.85;
const float kGodraySamples = 24.0;
const float kVignette = 0.32;

vec3 aces(vec3 x)
{
    return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

void main()
{
    vec2 uv = fragTexCoord;

    // Canicule : ondulation UV qui monte de l'écran.
    if (heat > 0.001)
        uv.x += sin(uv.y * 60.0 + time * 5.0) * 0.0022 * heat * (1.0 - uv.y);

    vec3 hdr = texture(texture0, uv).rgb + texture(glowTex, uv).rgb * kBloomStrength;

    // God rays : marche radiale sur le bloom depuis la position écran du soleil.
    if (godray > 0.001)
    {
        vec2 delta = (sunScreen - uv) / kGodraySamples;
        vec2 p = uv;
        float decay = 1.0;
        vec3 rays = vec3(0.0);
        for (int i = 0; i < int(kGodraySamples); ++i)
        {
            p += delta;
            rays += texture(glowTex, p).rgb * decay;
            decay *= 0.93;
        }
        hdr += rays / kGodraySamples * godray * 0.9;
    }

    vec3 col = aces(hdr);
    col = clamp(col * gradeGain + gradeLift, 0.0, 1.0);

    // Vignette douce + léger grain animé.
    float d = distance(fragTexCoord, vec2(0.5));
    col *= 1.0 - kVignette * smoothstep(0.45, 0.85, d);
    float g = fract(sin(dot(fragTexCoord * vec2(1920.0, 950.0) + time * 60.0, vec2(12.9898, 78.233))) * 43758.5453);
    col += (g - 0.5) * 0.012;

    finalColor = vec4(col, 1.0) * fragColor;
}
