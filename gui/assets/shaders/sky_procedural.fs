#version 330

// Ciel 100% procédural, piloté par la position réelle des astres (toSun/toMoon) :
//  - jour     : bleu saisonnier (skyDayColor), plus clair à l'horizon, diffusion
//               de Mie autour du soleil ;
//  - aube/soir: lueur chaude directionnelle qui suit l'azimut du soleil ;
//  - nuit     : palette saison + voie lactée, étoiles colorées à deux échelles,
//               étoiles filantes, nébuleuses FBM, halo lunaire ;
//  - aurores  : rideaux multicouches à rais verticaux, gradient vert -> pourpre.
// Les disques soleil/lune sont des billboards texturés dessinés après
// (drawCelestial). Utilise skybox.vs (fragPosition = direction du rayon).

in vec3 fragPosition;

uniform float time;
uniform vec3 toSun;         // direction VERS le soleil
uniform vec3 toMoon;
uniform vec3 sunGlowColor;  // couleur du halo (HDR)
uniform vec3 horizonColor;  // palette de NUIT de la saison
uniform vec3 zenithColor;
uniform vec3 skyDayColor;   // zénith du ciel de JOUR de la saison
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

// Une couche d'étoiles : cellules hachées, position sous-cellule, couleur par
// température (bleu-blanc / blanc / orangé), scintillement individuel.
vec3 starLayer(vec3 dir, float scale, float density, float t)
{
    vec3 sp = dir * scale;
    vec3 cell = floor(sp);
    float h = hash13(cell);
    if (h < density)
        return vec3(0.0);
    // Position de l'étoile dans sa cellule -> vraie forme de point.
    vec3 offs = vec3(hash13(cell + 17.1), hash13(cell + 31.7), hash13(cell + 47.3));
    float d = length(fract(sp) - clamp(offs, 0.15, 0.85));
    float core = exp(-d * d * 42.0);
    float twinkle = 0.70 + 0.30 * sin(t * (2.0 + 3.0 * hash13(cell + 5.0)) + h * 40.0);
    float temp = hash13(cell + 91.3);
    vec3 tint = temp < 0.25 ? vec3(1.05, 0.75, 0.55)   // géantes orangées
              : temp < 0.70 ? vec3(0.95, 0.95, 1.00)   // blanches
                            : vec3(0.75, 0.85, 1.20);  // bleues chaudes
    float mag = pow(hash13(cell + 3.7), 3.0); // peu de très brillantes
    return tint * core * twinkle * (0.5 + 2.5 * mag);
}

void main()
{
    vec3 dir = normalize(fragPosition);
    vec3 sunD = normalize(toSun);
    vec3 moonD = normalize(toMoon);

    // Phases pilotées par l'élévation réelle du soleil.
    float sunE = sunD.y;
    float day = smoothstep(-0.06, 0.28, sunE);
    float dusk = smoothstep(-0.22, -0.02, sunE) * (1.0 - smoothstep(0.10, 0.45, sunE));
    float night = 1.0 - smoothstep(-0.16, 0.04, sunE);

    // Azimut relatif au soleil : 1 côté soleil, 0 côté opposé.
    vec2 dirH = normalize(dir.xz + vec2(1e-5));
    vec2 sunH = normalize(sunD.xz + vec2(1e-5));
    float sunSide = dot(dirH, sunH) * 0.5 + 0.5;

    float h = clamp(dir.y * 0.5 + 0.5, 0.0, 1.0);

    // --- Nuit : palette de la saison ---
    vec3 col = mix(horizonColor, zenithColor, pow(h, 0.65));

    // --- Jour : gradient bleu saisonnier + éclaircissement à l'horizon ---
    vec3 dayHor = mix(skyDayColor, vec3(0.80, 0.88, 0.98), 0.62);
    vec3 dayCol = mix(dayHor, skyDayColor, pow(max(dir.y, 0.0), 0.45));
    // Diffusion de Mie : voile clair autour du soleil, surtout bas sur l'horizon.
    float sunDot = max(dot(dir, sunD), 0.0);
    dayCol += sunGlowColor * 0.05 * pow(sunDot, 4.0);
    col = mix(col, dayCol, day);

    // --- Aube / crépuscule : lueur chaude qui suit l'azimut du soleil ---
    float horizBand = exp(-max(dir.y, 0.0) * 4.5);
    vec3 duskWarm = mix(vec3(0.35, 0.12, 0.22), vec3(1.25, 0.45, 0.12), pow(sunSide, 2.0));
    col += duskWarm * dusk * horizBand * (0.18 + 0.82 * pow(sunSide, 3.0));
    // Bande "heure bleue" au-dessus de la lueur chaude.
    col += vec3(0.10, 0.14, 0.30) * dusk * exp(-abs(dir.y - 0.22) * 5.0) * 0.6;

    // --- Nébuleuses : deux couches de FBM qui dérivent lentement ---
    float starVis = starIntensity * (1.0 - day * 0.94);
    float neb = fbm(dir * 3.1 + vec3(time * 0.008, 0.0, time * 0.005));
    float neb2 = fbm(dir * 6.7 - vec3(0.0, time * 0.006, time * 0.004));
    float nebMask = smoothstep(0.45, 0.85, neb) * (0.5 + 0.5 * neb2);
    col += nebulaTint * nebMask * (0.15 + 0.55 * starVis);

    if (starVis > 0.02)
    {
        // --- Voie lactée : bande granuleuse le long d'un grand cercle incliné ---
        vec3 mwAxis = normalize(vec3(0.62, 0.33, 0.71));
        float mwDist = abs(dot(dir, mwAxis));
        float mwBand = exp(-mwDist * mwDist * 26.0);
        float mwTex = fbm(dir * 8.5 + 3.7) * 0.65 + fbm(dir * 21.0 - 1.3) * 0.35;
        vec3 mwCol = mix(vec3(0.42, 0.44, 0.62), vec3(0.72, 0.62, 0.55), mwTex);
        col += mwCol * mwBand * (0.25 + 0.75 * mwTex) * starVis * 0.55;
        // Poussières sombres au coeur de la bande.
        col -= vec3(0.10, 0.10, 0.12) * mwBand * smoothstep(0.55, 0.85, mwTex) * starVis * 0.5;

        // --- Étoiles : deux échelles + couleurs + scintillement ---
        col += starLayer(dir, 230.0, 0.9955, time) * starVis;
        col += starLayer(dir, 95.0, 0.9975, time * 0.7) * starVis * 1.4;

        // --- Étoile filante : un météore toutes les ~9 s, trajectoire hachée ---
        float epoch = floor(time / 9.0);
        float mf = fract(time / 9.0);
        if (mf < 0.22)
        {
            float h1 = hash13(vec3(epoch, 1.7, 9.2)), h2 = hash13(vec3(epoch, 5.3, 2.8));
            float h3 = hash13(vec3(epoch, 8.1, 4.6));
            vec3 sA = normalize(vec3(h1 - 0.5, 0.30 + 0.45 * h2, h3 - 0.5));
            vec3 sT = normalize(cross(sA, vec3(h2 - 0.5, 0.8, h1 - 0.5)));
            vec3 sN = cross(sA, sT);
            float u = dot(dir, sA), v = dot(dir, sT), w = dot(dir, sN);
            float phi = atan(v, u);              // position angulaire sur l'arc
            float head = mf / 0.22 * 0.55;       // tête du météore
            float behind = head - phi;           // >0 : dans la traînée
            float trail = (1.0 - smoothstep(0.0, 0.14, behind)) * step(0.0, behind);
            float streak = exp(-w * w * 22000.0) * trail * smoothstep(0.0, 0.02, phi);
            float fade = sin(clamp(mf / 0.22, 0.0, 1.0) * 3.14159); // fondu entrée/sortie
            col += vec3(1.0, 0.93, 0.78) * streak * fade * starVis * 2.2;
        }
    }

    // --- Aurores : rideaux multicouches, rais verticaux, gradient d'altitude ---
    float aur = auroraIntensity * (1.0 - day);
    if (aur > 0.004 && dir.y > -0.02)
    {
        float azA = atan(dir.x, -dir.z);                 // 0 = nord (-z)
        float north = 1.0 - smoothstep(0.6, 2.4, abs(azA)); // large secteur nord
        vec3 acc = vec3(0.0);
        for (int i = 0; i < 3; ++i)
        {
            float fi = float(i);
            // Ondulation lente du pied du rideau le long de l'azimut.
            float wave = fbm(vec3(azA * 1.6 + fi * 7.3, time * (0.022 + 0.011 * fi), fi * 3.1));
            float base = 0.05 + 0.15 * fi + 0.12 * wave;
            float y = dir.y - base;
            // Fin et net en bas, s'évanouit progressivement vers le haut.
            float body = smoothstep(0.0, 0.045, y) * exp(-max(y, 0.0) * 3.8);
            // Rais verticaux serrés qui dansent avec l'ondulation.
            float rays = pow(0.5 + 0.5 * sin(azA * 58.0 + wave * 15.0 + time * (0.30 + 0.22 * fi)), 2.0);
            rays = mix(0.30, 1.0, rays);
            // Respiration lente de l'intensité.
            float breathe = 0.60 + 0.40 * vnoise(vec3(azA * 2.6 + fi * 11.0, time * 0.16, fi));
            float k = body * rays * breathe;
            // Vert oxygène en bas -> turquoise -> pourpre/violet en altitude.
            vec3 ac = mix(vec3(0.06, 0.90, 0.32), vec3(0.10, 0.72, 0.58), clamp(y * 3.2, 0.0, 1.0));
            ac = mix(ac, vec3(0.48, 0.16, 0.72), clamp((y - 0.26) * 2.4, 0.0, 1.0));
            acc += ac * k * (0.55 - 0.13 * fi);
        }
        col += acc * north * aur * 1.9;
    }

    // --- Halos des astres (les disques texturés passent par-dessus) ---
    col += sunGlowColor * (pow(sunDot, 800.0) * 2.2 + pow(sunDot, 20.0) * 0.28 * (0.5 + 0.9 * dusk));
    float moonDot = max(dot(dir, moonD), 0.0);
    col += vec3(0.55, 0.65, 0.95) * (pow(moonDot, 900.0) * 1.1 + pow(moonDot, 38.0) * 0.12) * (1.0 - day * 0.85);

    // Orage : le flash illumine tout le ciel, surtout les nébuleuses.
    col += vec3(0.55, 0.55, 0.75) * lightning * (0.5 + nebMask);

    finalColor = vec4(col, 1.0);
}
