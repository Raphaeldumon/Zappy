// Tests unitaires du module environnement (aucune dépendance raylib).
#include "environment.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

static int failures = 0;
#define CHECK(cond)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(cond))                                                                                                   \
        {                                                                                                              \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                                       \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

static bool near(float a, float b, float eps = 0.02f)
{
    return std::fabs(a - b) < eps;
}

// Avance la simulation par pas de 16 ms jusqu'à ce que timeOfDay entre dans
// [lo, hi) (fenêtre sur le cycle 0..1).
static void advanceTo(env::EnvironmentState &e, float lo, float hi)
{
    for (int i = 0; i < 200000; ++i)
    {
        const float t = e.snap().timeOfDay;
        if (t >= lo && t < hi)
            return;
        e.update(0.016f);
    }
}

int main()
{
    // --- profileFor: saisons distinctes, fallback sur spring/clear ---
    const env::Profile w = env::profileFor("winter", "clear");
    const env::Profile sp = env::profileFor("spring", "clear");
    CHECK(w.particles.snow > 0.0f);
    CHECK(sp.particles.snow == 0.0f);
    CHECK(sp.particles.petals > 0.0f);
    CHECK(w.dayFraction < sp.dayFraction); // jours courts en hiver
    const env::Profile unk = env::profileFor("blorp", "blorp");
    CHECK(near(unk.groundMix, sp.groundMix));
    CHECK(near(unk.fogDensity, sp.fogDensity, 1e-6f));

    // --- météo: storm assombrit, fog densifie ---
    const env::Profile st = env::profileFor("summer", "storm");
    const env::Profile su = env::profileFor("summer", "clear");
    CHECK(st.sunTint.x < su.sunTint.x);
    CHECK(st.particles.rain > 0.9f);
    CHECK(env::profileFor("summer", "fog").fogDensity > su.fogDensity);

    // --- interpolation: converge vers la cible en ~10 s ---
    env::EnvironmentState e;
    e.setSeason("winter", "clear");
    for (int i = 0; i < 600; ++i)
        e.update(0.016f); // ~10 s
    CHECK(near(e.snap().groundMix, w.groundMix, 0.05f));
    CHECK(e.snap().particles.snow > 0.4f);

    // --- cycle jour/nuit ---
    env::EnvironmentState d;
    d.setSeason("spring", "clear");
    d.setTimeScale(40.0f);
    advanceTo(d, 0.48f, 0.52f); // midi
    CHECK(d.snap().sunVisibility > 0.9f);
    CHECK(d.snap().moonVisibility < 0.1f);
    CHECK(d.snap().sunColor.y > 0.7f);      // lumière blanche forte
    CHECK(d.snap().starIntensity < 0.3f);   // pas d'étoiles en plein jour
    CHECK(d.snap().sunDir.y < 0.0f);        // la lumière descend
    advanceTo(d, 0.995f, 1.0f);
    d.update(0.016f); // wrap
    advanceTo(d, 0.0f, 0.02f); // minuit
    CHECK(d.snap().sunVisibility < 0.1f);
    CHECK(d.snap().moonVisibility > 0.9f);
    CHECK(d.snap().starIntensity > 0.7f);
    CHECK(d.snap().sunColor.z > d.snap().sunColor.x); // clair de lune bleuté
    CHECK(d.snap().particles.fireflies > 0.3f);       // lucioles la nuit (spring)

    // --- trajectoire des astres: arc circulaire fidèle à la config ---
    // Élévation réelle (rad) d'une direction VERS l'astre.
    const auto elevOf = [](gfx::Vec3 lightDir) {
        const gfx::Vec3 to = gfx::scale(lightDir, -1.0f);
        return std::asin(to.y / gfx::length(to));
    };
    // Culmination à midi = kMaxSunElevation * sunArcHeight (spring: 1.13 rad).
    env::EnvironmentState arc;
    arc.setSeason("spring", "clear");
    arc.setTimeScale(40.0f);
    for (int i = 0; i < 600; ++i)
        arc.update(0.016f);
    advanceTo(arc, 0.498f, 0.502f);
    CHECK(near(elevOf(arc.snap().sunDir), 1.13f, 0.05f));
    // Lever plein est : à l'horizon, la direction est horizontale et sur +x.
    advanceTo(arc, 0.998f, 1.0f);
    arc.update(0.016f); // wrap
    advanceTo(arc, 0.222f, 0.228f); // sunrise spring (0.5 - 0.55/2)
    CHECK(std::fabs(elevOf(arc.snap().sunDir)) < 0.10f);
    CHECK(-arc.snap().sunDir.x > 0.9f);
    // Lune : culmine à minuit à 0.9 rad, pas plus haut.
    advanceTo(arc, 0.499f, 0.503f);
    advanceTo(arc, 0.998f, 1.0f);
    arc.update(0.016f);
    advanceTo(arc, 0.0f, 0.004f);
    CHECK(near(elevOf(arc.snap().moonDir), 0.9f, 0.05f));

    // --- déplacement continu et linéaire du lever au coucher ---
    // Un changement de saison en plein vol ne doit PAS déformer la trajectoire
    // visible : la vitesse angulaire reste constante, les nouveaux paramètres
    // (durée du jour, hauteur d'arc) s'appliquent au lever suivant.
    {
        const auto angleBetween = [](gfx::Vec3 a, gfx::Vec3 b) {
            const float d = std::clamp(a.x * b.x + a.y * b.y + a.z * b.z, -1.0f, 1.0f);
            return std::acos(d);
        };
        env::EnvironmentState lin;
        lin.setSeason("winter", "clear");
        lin.setTimeScale(1.0f);
        for (int i = 0; i < 2000; ++i)
            lin.update(0.016f); // profil hiver convergé
        advanceTo(lin, 0.40f, 0.405f); // soleil en vol (milieu de matinée)
        // Vitesse de référence en régime stable.
        gfx::Vec3 prev = lin.snap().sunDir;
        lin.update(0.016f);
        const float baseStep = angleBetween(prev, lin.snap().sunDir);
        // Changement de saison en plein vol : la position ne doit pas déraper.
        lin.setSeason("summer", "clear");
        float maxStep = 0.0f;
        for (int i = 0; i < 400; ++i) // ~6.4 s, couvre tout le fondu de profil
        {
            prev = lin.snap().sunDir;
            lin.update(0.016f);
            if (lin.snap().sunVisibility > 0.5f)
                maxStep = std::max(maxStep, angleBetween(prev, lin.snap().sunDir));
        }
        CHECK(maxStep < baseStep * 1.5f);
        // Les paramètres d'été s'appliquent bien au cycle suivant : culmination
        // haute (1.13 * 1.15) une fois le soleil recouché puis relevé.
        lin.setTimeScale(40.0f);
        advanceTo(lin, 0.05f, 0.10f); // nuit profonde : rafraîchissement
        advanceTo(lin, 0.498f, 0.502f);
        CHECK(near(elevOf(lin.snap().sunDir), 1.13f * 1.15f, 0.06f));
    }

    // Hiver : soleil rasant (arc bas), nettement plus bas qu'au printemps.
    env::EnvironmentState low;
    low.setSeason("winter", "clear");
    low.setTimeScale(40.0f);
    for (int i = 0; i < 600; ++i)
        low.update(0.016f);
    advanceTo(low, 0.498f, 0.502f);
    CHECK(near(elevOf(low.snap().sunDir), 1.13f * 0.45f, 0.06f));

    // --- aurores: hiver de nuit seulement ---
    env::EnvironmentState n;
    n.setSeason("winter", "clear");
    n.setTimeScale(40.0f);
    for (int i = 0; i < 600; ++i)
        n.update(0.016f); // laisser le profil converger
    advanceTo(n, 0.0f, 0.02f);
    CHECK(n.snap().auroraIntensity > 0.3f);
    advanceTo(n, 0.48f, 0.52f);
    CHECK(n.snap().auroraIntensity < 0.15f);

    if (failures == 0)
        std::printf("gui_environment: all tests passed\n");
    return failures == 0 ? 0 : 1;
}
