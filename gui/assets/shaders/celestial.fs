#version 330

// Billboard d'astre : Luan EST le soleil, Palasse EST la lune. Le quad porte
// un disque procédural (limbe assombri + couronne) et le portrait est composé
// PAR-DESSUS avec une luminosité bornée pour rester lisible après l'ACES —
// multiplier la texture par l'émissif HDR la cramait en blanc.

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec3 emissive;   // teinte HDR du disque et du halo
uniform float bodyAlpha;
uniform int isMoon;      // 0 = Luan (soleil), 1 = Palasse (lune)
uniform float time;

out vec4 finalColor;

void main()
{
    vec2 p = fragTexCoord - vec2(0.5);
    float r = length(p);
    float discR = (isMoon == 1) ? 0.30 : 0.27;
    float inDisc = 1.0 - smoothstep(discR * 0.965, discR, r);

    // Portrait plaqué sur la face du disque (marge pour ne pas toucher le bord).
    vec2 faceUv = p / (discR * 1.9) + vec2(0.5);
    vec4 tex = texture(texture0, clamp(faceUv, 0.0, 1.0));
    float faceIn = step(abs(faceUv.x - 0.5), 0.5) * step(abs(faceUv.y - 0.5), 0.5);

    vec3 col;
    float alpha;
    if (isMoon == 1)
    {
        // Surface lunaire froide, limbe assombri.
        float limb = 1.0 - 0.42 * smoothstep(0.35, 1.0, r / discR);
        vec3 surfaceCol = emissive * 0.85 * limb;
        // Photo de Palasse (fond blanc) : découpe circulaire dans le disque,
        // teintée clair-de-lune, fondue sur les bords.
        float faceMask = (1.0 - smoothstep(0.40, 0.50, length(faceUv - vec2(0.5)))) * tex.a * faceIn;
        vec3 portrait = tex.rgb * vec3(0.88, 0.95, 1.12) * 1.05;
        col = mix(surfaceCol, portrait * limb, faceMask * 0.92);
        // Halo froid discret autour du disque.
        float glow = exp(-(max(r - discR, 0.0)) * 14.0) * (1.0 - inDisc);
        col = mix(emissive * 0.75, col, inDisc);
        alpha = max(inDisc, glow * 0.40);
    }
    else
    {
        // Disque incandescent : limbe assombri, granulation qui frémit.
        float limb = 1.0 - 0.45 * pow(clamp(r / discR, 0.0, 1.0), 2.4);
        float granule = 0.94 + 0.06 * sin(p.x * 90.0 + time * 0.9) * sin(p.y * 90.0 - time * 0.7);
        vec3 discCol = emissive * limb * granule;
        // Luan détouré (alpha) : silhouette dorée lisible sur la fournaise,
        // luminosité indépendante de l'émissif HDR.
        vec3 portrait = tex.rgb * vec3(1.55, 1.25, 0.85) + vec3(0.30, 0.16, 0.04);
        col = mix(discCol, portrait, tex.a * faceIn * inDisc * 0.90);
        // Couronne : décroissance + protubérances angulaires lentes.
        float ang = atan(p.y, p.x);
        float flare = 0.75 + 0.25 * sin(ang * 9.0 + time * 0.25) * sin(ang * 5.0 - time * 0.17);
        float corona = exp(-(max(r - discR, 0.0)) * 9.0) * flare * (1.0 - inDisc);
        col = mix(emissive * 1.05, col, inDisc);
        alpha = max(inDisc, corona * 0.55);
    }

    alpha *= bodyAlpha;
    if (alpha < 0.01)
        discard;
    finalColor = vec4(col, alpha);
}
