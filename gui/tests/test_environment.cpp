// Tests unitaires du module environnement (aucune dépendance raylib).
#include "environment.hpp"
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
