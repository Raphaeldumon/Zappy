#version 330

// Ciel 100% procédural (remplace Background.png) : dégradé horizon->zénith,
// étoiles scintillantes, nébuleuses FBM animées, aurores boréales, halos
// soleil/lune. Les disques eux-mêmes sont des billboards texturés dessinés
// après (drawCelestial). Utilise le skybox.vs existant (fragPosition = ray).

in vec3 fragPosition;

uniform float time;
uniform vec3 toSun;         // direction VERS le soleil
uniform vec3 toMoon;
uniform vec3 sunGlowColor;  // couleur du halo (HDR)
uniform vec3 horizonColor;
uniform vec3 zenithColor;
uniform vec3 nebulaTint;
uniform float starIntensity;
uniform float auroraIntensity;
uniform float lightning;    // 0..1 flash d'orage

out vec4 finalColor;

float hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.zyx + 31.32);
    return fract((p.x + p.y) * p.z);
}

float vnoise(vec3 p)
{
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float n000 = hash13(i), n100 = hash13(i + vec3(1, 0, 0));
    float n010 = hash13(i + vec3(0, 1, 0)), n110 = hash13(i + vec3(1, 1, 0));
    float n001 = hash13(i + vec3(0, 0, 1)), n101 = hash13(i + vec3(1, 0, 1));
    float n011 = hash13(i + vec3(0, 1, 1)), n111 = hash13(i + vec3(1, 1, 1));
    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

float fbm(vec3 p)
{
    float a = 0.5, s = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        s += a * vnoise(p);
        p *= 2.03;
        a *= 0.5;
    }
    return s;
}

void main()
{
    vec3 dir = normalize(fragPosition);

    // Dégradé de fond : horizon chaud -> zénith profond.
    float h = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 col = mix(horizonColor, zenithColor, pow(h, 0.65));

    // Nébuleuses : deux couches de FBM qui dérivent lentement.
    float neb = fbm(dir * 3.1 + vec3(time * 0.008, 0.0, time * 0.005));
    float neb2 = fbm(dir * 6.7 - vec3(0.0, time * 0.006, time * 0.004));
    float nebMask = smoothstep(0.45, 0.85, neb) * (0.5 + 0.5 * neb2);
    col += nebulaTint * nebMask * (0.35 + 0.45 * starIntensity);

    // Étoiles : grille hachée, seuil dur, scintillement temporel.
    vec3 sp = dir * 220.0;
    float star = hash13(floor(sp));
    float twinkle = 0.75 + 0.25 * sin(time * 3.0 + star * 40.0);
    float starMask = smoothstep(0.997, 1.0, star) * twinkle;
    col += vec3(0.9, 0.95, 1.1) * starMask * starIntensity * 2.2;

    // Aurores : rideaux ondulants vert/cyan au-dessus de l'horizon nord (-z).
    if (auroraIntensity > 0.001 && dir.y > 0.02 && dir.z < 0.3)
    {
        float band = fbm(vec3(dir.x * 2.4, dir.y * 5.0 - time * 0.05, dir.z * 2.4));
        float curtain = smoothstep(0.5, 0.9, band) * smoothstep(0.02, 0.25, dir.y) * smoothstep(0.9, 0.2, dir.y);
        vec3 auroraCol = mix(vec3(0.1, 0.9, 0.45), vec3(0.2, 0.5, 0.9), fbm(dir * 3.0 + time * 0.02));
        col += auroraCol * curtain * auroraIntensity * 1.6;
    }

    // Halos des astres (les disques texturés sont dessinés par-dessus).
    float sunDot = max(dot(dir, normalize(toSun)), 0.0);
    col += sunGlowColor * (pow(sunDot, 900.0) * 3.0 + pow(sunDot, 24.0) * 0.35);
    float moonDot = max(dot(dir, normalize(toMoon)), 0.0);
    col += vec3(0.5, 0.6, 0.9) * (pow(moonDot, 1200.0) * 1.2 + pow(moonDot, 40.0) * 0.10) * starIntensity;

    // Orage : le flash illumine tout le ciel, surtout les nébuleuses.
    col += vec3(0.55, 0.55, 0.75) * lightning * (0.5 + nebMask);

    finalColor = vec4(col, 1.0);
}
