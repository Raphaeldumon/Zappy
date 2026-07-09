#version 330

// Sol du monde : éclairage (même modèle que lighting.fs + spéculaire mouillé),
// teinte saisonnière, anti-tiling, grille de tuiles AA, et couverture
// dynamique échantillonnée depuis coverMap (accumulation CPU) :
//   R = neige, G = litière de feuilles, B = eau/flaques, A = traces pressées.

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
uniform int shadowsOn; // 0 tant qu'aucune passe n'a tourné
uniform int shadowMapRes;

// Couverture dynamique (neige/feuilles/eau/traces) + habillage procédural.
uniform sampler2D coverMap;
uniform int coverOn;    // 0 = pas de carte (mode tore / désactivé)
uniform vec2 worldSize; // dimensions monde de la carte (uv = pos.xz / worldSize)
uniform float tileSize; // arête d'une tuile en unités monde
uniform float time;     // secondes, anime flaques et scintillement

out vec4 finalColor;

// ---------------------------------------------------------------------------
// Bruit
// ---------------------------------------------------------------------------
float hash21(vec2 p)
{
    p = fract(p * vec2(234.34, 435.345));
    p += dot(p, p + 34.23);
    return fract(p.x * p.y);
}

float vnoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1, 0));
    float c = hash21(i + vec2(0, 1));
    float d = hash21(i + vec2(1, 1));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// 2 octaves suffisent : le détail fin vient de la texture d'herbe.
float fbm(vec2 p)
{
    return 0.65 * vnoise(p) + 0.35 * vnoise(p * 2.7 + 13.7);
}

// ---------------------------------------------------------------------------
// Ombre portée PCF 3x3 depuis la depth map de la lumière active.
// ---------------------------------------------------------------------------
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
    float ts = max(tileSize, 1.0);
    vec2 wp = fragPosition.xz;
    vec2 tile = floor(wp / ts);
    float tileHash = hash21(tile + 0.5);

    // --- Anti-tiling : l'UV de chaque tuile est tourné de k*90° + miroir ---
    vec2 uv = fragTexCoord;
    if (coverOn == 1)
    {
        float r = floor(tileHash * 4.0);
        if (r == 1.0)
            uv = vec2(1.0 - uv.y, uv.x);
        else if (r == 2.0)
            uv = 1.0 - uv;
        else if (r == 3.0)
            uv = vec2(uv.y, 1.0 - uv.x);
        if (fract(tileHash * 16.0) > 0.5)
            uv.x = 1.0 - uv.x;
    }
    vec4 texel = texture(texture0, uv) * colDiffuse * fragColor;
    vec3 base = texel.rgb;

    // --- Variation macro : plaques d'herbe sèche/dense qui cassent le damier ---
    float macro = fbm(wp / (ts * 3.1));
    base *= 0.86 + 0.26 * macro;                              // luminance par plaques
    base = mix(base, base * vec3(1.08, 1.02, 0.80), // jaunissement des creux
               smoothstep(0.55, 0.85, macro) * 0.35);
    base *= 0.94 + 0.12 * tileHash; // légère identité par tuile

    // --- Saison : désature la texture vers la teinte du profil (or, roux...) ---
    float lum = dot(base, vec3(0.299, 0.587, 0.114));
    base = mix(base, groundOverlay * (0.35 + 0.9 * lum), groundMix);

    // --- Couverture dynamique ---
    float snow = 0.0, leaf = 0.0, wet = 0.0, trail = 0.0;
    if (coverOn == 1)
    {
        vec4 cov = texture(coverMap, wp / worldSize);
        snow = cov.r;
        leaf = cov.g;
        wet = cov.b;
        trail = cov.a;
    }

    // Bruits partagés par les couches (échelle "débris au sol").
    float nFine = vnoise(wp / ts * 9.0);
    float nMid = vnoise(wp / ts * 3.3 + 7.7);

    // --- Litière de feuilles : taches orangées qui gagnent du terrain ---
    if (leaf > 0.003)
    {
        // Couverture à seuil : les feuilles s'installent d'abord dans les creux
        // du bruit, puis le tapis se referme quand leaf -> 1.
        float litter = smoothstep(nFine * 0.85, nFine * 0.85 + 0.35, leaf * 1.25);
        // Trois tons d'automne choisis par le bruit moyen (roux/brun/doré).
        vec3 leafCol = mix(vec3(0.62, 0.30, 0.10), vec3(0.42, 0.22, 0.08), smoothstep(0.3, 0.7, nMid));
        leafCol = mix(leafCol, vec3(0.72, 0.48, 0.12), smoothstep(0.75, 0.95, nFine));
        leafCol *= 0.85 + 0.3 * nFine; // relief feuille à feuille
        base = mix(base, leafCol, litter * 0.95);
        // Sentier balayé : le passage assombrit la litière restante (terre nue).
        base *= 1.0 - 0.22 * trail * litter;
    }

    // --- Eau : sol détrempé, puis flaques miroir dans les creux ---
    float puddle = 0.0;
    if (wet > 0.003)
    {
        base *= 1.0 - 0.42 * wet;            // sol sombre quand il est trempé
        base = mix(base, base * vec3(0.9, 0.95, 1.05), wet * 0.5); // reflet froid
        puddle = smoothstep(0.55, 0.9, wet * (0.55 + 0.6 * (1.0 - nMid)));
        // Boue sur les sentiers mouillés.
        base *= 1.0 - 0.25 * trail * wet;
    }

    // --- Neige : gagne depuis les creux, éteint les couches du dessous ---
    float snowCover = 0.0;
    if (snow > 0.003)
    {
        snowCover = smoothstep(nFine * 0.9, nFine * 0.9 + 0.45, snow * 1.35);
        vec3 snowCol = vec3(0.88, 0.92, 1.0) * (0.85 + 0.15 * nMid);
        // Traces : neige compactée, plus sombre et bleutée (pas de re-fonte).
        snowCol *= 1.0 - 0.38 * trail;
        snowCol = mix(snowCol, snowCol * vec3(0.82, 0.88, 1.05), trail);
        base = mix(base, snowCol, snowCover);
        puddle *= 1.0 - snowCover; // pas de flaque sous la neige
    }

    // --- Grille de tuiles : fine ligne AA, effacée par la neige/l'eau ---
    if (coverOn == 1)
    {
        vec2 g = abs(fract(wp / ts) - 0.5);
        float edge = 0.5 - max(g.x, g.y);              // 0 pile sur le bord
        float aa = fwidth(edge) * 1.5;
        float line = 1.0 - smoothstep(0.006, 0.006 + aa, edge);
        float hide = max(snowCover, max(puddle, leaf * 0.6));
        base *= 1.0 - 0.30 * line * (1.0 - hide);
    }

    // --- Éclairage ---
    vec3 n = normalize(fragNormal);
    vec3 l = -normalize(sunDir);
    vec3 v = normalize(viewPos - fragPosition);
    float diff = max(dot(n, l), 0.0);
    float shadow = shadowFactor(fragPosition, n, l);

    // Flaques : le miroir renvoie le ciel (brume + soleil) et une ondulation
    // anime doucement la surface tant qu'elle existe.
    if (puddle > 0.0)
    {
        vec3 skyRef = fogColor * 1.6 + sunColor * 0.22;
        base = mix(base, skyRef, puddle * 0.65);
        float ripple = sin(dot(wp, vec2(0.35, 0.29)) + time * 2.1) * sin(dot(wp, vec2(-0.27, 0.4)) + time * 1.6);
        n = normalize(n + vec3(ripple * 0.035 * puddle, 0.0, ripple * 0.028 * puddle));
    }

    vec3 lit = base * (ambientColor + sunColor * diff * shadow);

    // Spéculaire : fort sur l'eau, doux sur la neige, nul sur sol sec.
    float wetSpec = max(puddle, wet * 0.45);
    float specAmt = wetSpec * 0.9 + snowCover * 0.12;
    if (specAmt > 0.001)
    {
        vec3 h = normalize(l + v);
        float spec = pow(max(dot(n, h), 0.0), mix(24.0, 90.0, puddle));
        lit += sunColor * spec * specAmt * shadow;
    }

    // Scintillement de la neige : points qui s'allument selon l'angle de vue.
    if (snowCover > 0.2)
    {
        float glint = step(0.988, hash21(floor(wp * 2.3) + floor(time * 3.0) * 0.37));
        lit += sunColor * glint * snowCover * (1.0 - trail) * 0.8 * diff * shadow;
    }

    float dist = length(viewPos - fragPosition);
    float fogT = clamp(exp(-fogDensity * dist), 0.0, 1.0);
    finalColor = vec4(mix(fogColor, lit, fogT), texel.a);
}
