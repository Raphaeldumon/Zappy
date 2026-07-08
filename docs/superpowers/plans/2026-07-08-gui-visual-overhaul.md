# GUI Visual Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remplacer les effets saison/météo 2D et l'environnement statique du GUI Zappy par un rendu dynamique : ciel procédural, cycle jour/nuit (Luan = soleil, Palasse = lune), lumière + ombres pilotées par la saison, particules 3D, post-process ACES.

**Architecture:** Un module central raylib-free `env::EnvironmentState` (gui/environment.{hpp,cpp}) interpole tous les paramètres visuels par (saison, météo) et calcule la position soleil/lune depuis une horloge temps réel (~180 s/cycle). Le rendu lit uniquement son `Snapshot` : le ciel (shader), la lumière (uniforms), les particules (sim CPU + billboards), le post-process (RT 16F + composite) et le sol. Spec : `docs/superpowers/specs/2026-07-08-gui-visual-overhaul-design.md`.

**Tech Stack:** C++17, raylib + rlgl, GLSL 330, CMake (target `zappy_gui`), ctest pour le module logique.

## Global Constraints

- Moteur raylib conservé ; raylib inclus UNIQUEMENT dans `gui/raylibWrapper.cpp` (PIMPL). `environment.{hpp,cpp}` et `particles.{hpp,cpp}` n'incluent jamais raylib — seulement `gfxTypes.hpp` (+ `raylibWrapper.hpp` pour particles.cpp qui appelle le facade).
- Chaque capacité GPU (shader, FBO, texture) a un fallback : échec → `gfx::logWarn` + chemin de rendu précédent conservé (même stratégie que `enableBloom`).
- Chaque tâche laisse `zappy_gui` compilable et jouable.
- Performance : full max, pas de mode qualité.
- Shaders GLSL `#version 330`, dans `gui/assets/shaders/`.
- Assets : `gui/assets/luan.png` (soleil, fond transparent), `gui/assets/palasse.png` (lune, fond blanc → masque circulaire shader).
- Build : `make zappy_gui` à la racine (sortie `./zappy_gui`). Tests logiques : `ctest --test-dir build -R gui_environment`.
- Vérification visuelle : `./zappy_server -p 4242 -x 12 -y 12 -n alpha beta -c 3 -f 50 &` puis `( cd gui && ../zappy_gui -p 4242 -h 127.0.0.1 )`. Touches debug (ajoutées Tâche 2) : F5 force la saison, F6 force la météo, F7 accélère l'horloge jour/nuit ×40.
- Style commits : messages courts type `feat: ...` / `fix: ...` (voir `git log`), co-author Claude.

## File Map

| Fichier | Rôle |
|---|---|
| Create `gui/environment.hpp/.cpp` | Profils (saison, météo), interpolation, horloge jour/nuit, Snapshot |
| Create `gui/tests/test_environment.cpp` | Tests unitaires (asserts, sans raylib) |
| Create `gui/particles.hpp/.cpp` | Sim CPU + rendu billboards des particules 3D |
| Create `gui/assets/shaders/sky_procedural.fs` | Ciel : étoiles, nébuleuses FBM, aurores, glows |
| Create `gui/assets/shaders/celestial.fs` | Billboards soleil/lune (masque circulaire, émissif HDR) |
| Create `gui/assets/shaders/floor.vs/.fs` | Sol batché : lumière/fog/ombres/teinte saison |
| Create `gui/assets/shaders/post_composite.fs` | ACES + god rays + grading + vignette + heat + grain |
| Modify `gui/assets/shaders/lighting.fs` | Constantes → uniforms, rim light, fog, ombres PCF |
| Modify `gui/raylibWrapper.hpp/.cpp` | setSceneLighting, sky procédural, celestial, RT 16F, post params, shadow pass, blend additive, texture radiale |
| Modify `gui/interface.hpp/.cpp` | Intégration env/particules, drawWorld3D(depthPass), suppression drawWeatherOverlay, touches debug |
| Modify `gui/gfxTypes.hpp` | Touches F5/F6/F7 |
| Modify `gui/CMakeLists.txt` | Nouvelles sources + test ctest |

---

### Task 1: Module EnvironmentState (logique pure + tests)

**Files:**
- Create: `gui/environment.hpp`
- Create: `gui/environment.cpp`
- Create: `gui/tests/test_environment.cpp`
- Modify: `gui/CMakeLists.txt`

**Interfaces:**
- Consumes: `gfxTypes.hpp` (Vec3, helpers).
- Produces: `env::EnvironmentState` avec `void setSeason(const std::string&, const std::string&)`, `void update(float dt)`, `const env::Snapshot& snap() const`, `void setTimeScale(float)`, `float timeScale() const` ; `env::Profile profileFor(const std::string& season, const std::string& weather)` ; structs `env::Snapshot`, `env::ParticleProfile` (champs exacts ci-dessous). Toutes les tâches suivantes consomment `Snapshot`.

- [ ] **Step 1: Écrire le test qui échoue**

Créer `gui/tests/test_environment.cpp` :

```cpp
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
```

Dans `gui/CMakeLists.txt`, insérer AVANT `find_library(RAYLIB_LIBRARY ...)` (le test ne dépend pas de raylib et doit exister même sans elle) :

```cmake
# Tests du module environnement (logique pure, aucune dépendance raylib).
add_executable(zappy_gui_environment_tests
    tests/test_environment.cpp
    environment.cpp)
set_target_properties(zappy_gui_environment_tests PROPERTIES
    CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
target_include_directories(zappy_gui_environment_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(zappy_gui_environment_tests PRIVATE -Wall -Wextra)
add_test(NAME gui_environment COMMAND zappy_gui_environment_tests)
```

- [ ] **Step 2: Vérifier que le test échoue**

Run: `make zappy_gui 2>&1 | tail -5` (reconfigure CMake) puis `cmake --build build --target zappy_gui_environment_tests 2>&1 | tail -5`
Expected: FAIL — `environment.hpp: No such file or directory`.

- [ ] **Step 3: Écrire environment.hpp**

Créer `gui/environment.hpp` :

```cpp
#pragma once

// ---------------------------------------------------------------------------
// env — état visuel ambiant du monde (saison, météo, cycle jour/nuit).
// Module raylib-free : le rendu (ciel, lumière, particules, post, sol) ne lit
// que le Snapshot interpolé ; personne d'autre ne connaît les saisons.
// ---------------------------------------------------------------------------

#include "gfxTypes.hpp"
#include <string>

namespace env
{

// Densités 0..1 par effet (0 = coupé) ; vent en unités monde / seconde.
struct ParticleProfile
{
    float snow{0}, leaves{0}, petals{0}, fireflies{0}, dust{0}, embers{0}, rain{0}, spores{0};
    gfx::Vec3 wind{0, 0, 0};
};

// Description statique d'un couple (saison, météo). Les couleurs sont des
// facteurs linéaires 0..1+ (HDR autorisé), pas des gfx::Color.
struct Profile
{
    gfx::Vec3 sunTint{1, 1, 1};     // multiplie la courbe jour/nuit du soleil
    gfx::Vec3 ambientTint{1, 1, 1};
    gfx::Vec3 skyHorizon{}, skyZenith{}, nebulaTint{};
    gfx::Vec3 fogColor{};
    float fogDensity{0};            // fog exponentiel: transmit = exp(-d * dist)
    gfx::Vec3 gradeLift{0, 0, 0}, gradeGain{1, 1, 1};
    gfx::Vec3 groundOverlay{1, 1, 1}; // teinte saison mélangée sur le sol
    float groundMix{0};               // 0 = texture pure, 1 = overlay pur
    float dayFraction{0.5f};          // part du cycle où le soleil est levé
    float sunArcHeight{1.0f};         // 1.0 = élévation max ~65 deg
    float auroraMax{0};
    float heatDistort{0};
    ParticleProfile particles;
};

// Sortie interpolée par frame, prête à être poussée en uniforms.
struct Snapshot
{
    gfx::Vec3 sunDir{}, moonDir{};  // direction de PROPAGATION de la lumière
    gfx::Vec3 activeLightDir{};     // sunDir le jour, moonDir la nuit (ombres)
    gfx::Vec3 sunColor{};           // lumière directionnelle combinée soleil+lune (HDR)
    gfx::Vec3 ambient{};
    float sunVisibility{0}, moonVisibility{0}; // alpha des billboards, 0..1
    gfx::Vec3 skyHorizon{}, skyZenith{}, nebulaTint{};
    float starIntensity{0}, auroraIntensity{0}, lightningFlash{0};
    gfx::Vec3 fogColor{};
    float fogDensity{0};
    gfx::Vec3 gradeLift{}, gradeGain{1, 1, 1};
    float heatDistort{0};
    gfx::Vec3 groundOverlay{1, 1, 1};
    float groundMix{0};
    ParticleProfile particles;
    float timeOfDay{0.35f}; // 0 = minuit, 0.5 = midi
};

// Table (saison, météo) -> Profile ; noms inconnus => spring / clear.
Profile profileFor(const std::string &season, const std::string &weather);

class EnvironmentState
{
  public:
    // Change la cible d'interpolation (transition douce ~4 s, jamais de saut).
    void setSeason(const std::string &season, const std::string &weather);
    void update(float dt);
    const Snapshot &snap() const
    {
        return _snap;
    }
    void setTimeScale(float s)
    {
        _timeScale = s;
    }
    float timeScale() const
    {
        return _timeScale;
    }

  private:
    std::string _season{"spring"}, _weather{"clear"};
    Profile _cur{profileFor("spring", "clear")};
    Profile _tgt{_cur};
    Snapshot _snap{};
    float _timeOfDay{0.35f}; // démarre en matinée
    float _timeScale{1.0f};
    float _flash{0};
    float _nextFlashIn{4.0f};
};

} // namespace env
```

- [ ] **Step 4: Écrire environment.cpp**

Créer `gui/environment.cpp` :

```cpp
#include "environment.hpp"
#include <algorithm>
#include <cmath>

namespace env
{
namespace
{

constexpr float kPi = 3.14159265358979f;
constexpr float kDayLengthSeconds = 180.0f; // un cycle jour+nuit complet
constexpr float kBlendTau = 1.2f;           // constante de temps du fondu de profil
constexpr float kMaxSunElevation = 1.13f;   // ~65 deg, modulé par sunArcHeight

float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}
gfx::Vec3 lerp3(gfx::Vec3 a, gfx::Vec3 b, float t)
{
    return {lerpf(a.x, b.x, t), lerpf(a.y, b.y, t), lerpf(a.z, b.z, t)};
}
gfx::Vec3 mul3(gfx::Vec3 a, gfx::Vec3 b)
{
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}
float smooth01(float e0, float e1, float x)
{
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

Profile seasonBase(const std::string &s)
{
    Profile p{};
    if (s == "summer")
    {
        p.sunTint = {1.08f, 1.00f, 0.88f};
        p.skyHorizon = {0.20f, 0.12f, 0.10f};
        p.skyZenith = {0.04f, 0.03f, 0.06f};
        p.nebulaTint = {0.75f, 0.55f, 0.22f};
        p.fogColor = {0.55f, 0.50f, 0.40f};
        p.fogDensity = 0.00025f;
        p.gradeGain = {1.06f, 1.00f, 0.94f};
        p.groundOverlay = {0.80f, 0.72f, 0.30f};
        p.groundMix = 0.35f;
        p.dayFraction = 0.68f;
        p.sunArcHeight = 1.15f;
        p.particles.dust = 0.45f;
        p.particles.wind = {3, 0, 1};
        return p;
    }
    if (s == "autumn")
    {
        p.sunTint = {1.12f, 0.85f, 0.60f};
        p.skyHorizon = {0.22f, 0.10f, 0.06f};
        p.skyZenith = {0.04f, 0.02f, 0.06f};
        p.nebulaTint = {0.70f, 0.32f, 0.14f};
        p.fogColor = {0.50f, 0.42f, 0.35f};
        p.fogDensity = 0.0005f;
        p.gradeGain = {1.08f, 0.96f, 0.86f};
        p.groundOverlay = {0.72f, 0.45f, 0.18f};
        p.groundMix = 0.45f;
        p.dayFraction = 0.45f;
        p.sunArcHeight = 0.70f;
        p.particles.leaves = 0.7f;
        p.particles.wind = {14, 0, 5};
        return p;
    }
    if (s == "winter")
    {
        p.sunTint = {0.85f, 0.92f, 1.08f};
        p.ambientTint = {0.95f, 1.0f, 1.1f};
        p.skyHorizon = {0.10f, 0.14f, 0.22f};
        p.skyZenith = {0.02f, 0.03f, 0.07f};
        p.nebulaTint = {0.25f, 0.45f, 0.75f};
        p.fogColor = {0.60f, 0.68f, 0.78f};
        p.fogDensity = 0.0009f;
        p.gradeLift = {0.02f, 0.03f, 0.05f};
        p.gradeGain = {0.94f, 0.98f, 1.06f};
        p.groundOverlay = {0.92f, 0.95f, 1.00f};
        p.groundMix = 0.78f;
        p.dayFraction = 0.35f;
        p.sunArcHeight = 0.45f;
        p.auroraMax = 0.8f;
        p.particles.snow = 0.6f;
        p.particles.wind = {8, 0, 3};
        return p;
    }
    // spring (défaut, y compris noms inconnus)
    p.sunTint = {1.00f, 0.98f, 0.95f};
    p.skyHorizon = {0.16f, 0.10f, 0.22f};
    p.skyZenith = {0.03f, 0.02f, 0.08f};
    p.nebulaTint = {0.45f, 0.20f, 0.55f};
    p.fogColor = {0.45f, 0.50f, 0.60f};
    p.fogDensity = 0.00035f;
    p.gradeLift = {0.01f, 0.00f, 0.02f};
    p.gradeGain = {1.03f, 1.00f, 1.04f};
    p.groundOverlay = {0.30f, 0.55f, 0.25f};
    p.groundMix = 0.12f;
    p.dayFraction = 0.55f;
    p.sunArcHeight = 1.0f;
    p.particles.petals = 0.5f;
    p.particles.fireflies = 0.6f;
    p.particles.wind = {6, 0, 2};
    return p;
}

void applyWeather(Profile &p, const std::string &w)
{
    if (w == "rain")
    {
        p.particles.rain = 0.7f;
        p.sunTint = gfx::scale(p.sunTint, 0.55f);
        p.ambientTint = gfx::scale(p.ambientTint, 0.80f);
        p.fogDensity += 0.0006f;
        p.fogColor = {0.30f, 0.34f, 0.40f};
        p.gradeGain = mul3(p.gradeGain, {0.90f, 0.95f, 1.00f});
    }
    else if (w == "storm")
    {
        p.particles.rain = 1.0f;
        p.particles.wind = gfx::add(p.particles.wind, {18, 0, 6});
        p.sunTint = gfx::scale(p.sunTint, 0.30f);
        p.ambientTint = gfx::scale(p.ambientTint, 0.60f);
        p.fogDensity += 0.0010f;
        p.fogColor = {0.16f, 0.17f, 0.24f};
        p.gradeGain = mul3(p.gradeGain, {0.80f, 0.85f, 0.95f});
    }
    else if (w == "fog")
    {
        p.fogDensity += 0.0022f;
        p.fogColor = {0.75f, 0.78f, 0.80f};
        p.skyHorizon = lerp3(p.skyHorizon, p.fogColor, 0.7f);
        p.sunTint = gfx::scale(p.sunTint, 0.70f);
    }
    else if (w == "heat")
    {
        p.heatDistort = 0.6f;
        p.particles.embers = 0.5f;
        p.sunTint = mul3(p.sunTint, {1.25f, 0.85f, 0.55f});
        p.gradeGain = mul3(p.gradeGain, {1.12f, 0.95f, 0.80f});
    }
    else if (w == "fertile")
    {
        p.particles.spores = 0.6f;
        p.gradeGain = mul3(p.gradeGain, {0.95f, 1.08f, 0.95f});
        p.nebulaTint = lerp3(p.nebulaTint, {0.20f, 0.80f, 0.40f}, 0.5f);
    }
    // "clear" / inconnu : profil saison inchangé
}

void lerpParticles(ParticleProfile &c, const ParticleProfile &t, float k)
{
    c.snow = lerpf(c.snow, t.snow, k);
    c.leaves = lerpf(c.leaves, t.leaves, k);
    c.petals = lerpf(c.petals, t.petals, k);
    c.fireflies = lerpf(c.fireflies, t.fireflies, k);
    c.dust = lerpf(c.dust, t.dust, k);
    c.embers = lerpf(c.embers, t.embers, k);
    c.rain = lerpf(c.rain, t.rain, k);
    c.spores = lerpf(c.spores, t.spores, k);
    c.wind = lerp3(c.wind, t.wind, k);
}

void lerpProfile(Profile &c, const Profile &t, float k)
{
    c.sunTint = lerp3(c.sunTint, t.sunTint, k);
    c.ambientTint = lerp3(c.ambientTint, t.ambientTint, k);
    c.skyHorizon = lerp3(c.skyHorizon, t.skyHorizon, k);
    c.skyZenith = lerp3(c.skyZenith, t.skyZenith, k);
    c.nebulaTint = lerp3(c.nebulaTint, t.nebulaTint, k);
    c.fogColor = lerp3(c.fogColor, t.fogColor, k);
    c.fogDensity = lerpf(c.fogDensity, t.fogDensity, k);
    c.gradeLift = lerp3(c.gradeLift, t.gradeLift, k);
    c.gradeGain = lerp3(c.gradeGain, t.gradeGain, k);
    c.groundOverlay = lerp3(c.groundOverlay, t.groundOverlay, k);
    c.groundMix = lerpf(c.groundMix, t.groundMix, k);
    c.dayFraction = lerpf(c.dayFraction, t.dayFraction, k);
    c.sunArcHeight = lerpf(c.sunArcHeight, t.sunArcHeight, k);
    c.auroraMax = lerpf(c.auroraMax, t.auroraMax, k);
    c.heatDistort = lerpf(c.heatDistort, t.heatDistort, k);
    lerpParticles(c.particles, t.particles, k);
}

// Élévation normalisée d'un astre : 1 au zénith de sa période, 0 à l'horizon,
// négatif dessous. centre = heure du zénith, fraction = durée de la période.
float elevation01(float t, float centre, float fraction)
{
    float d = t - centre;
    d -= std::round(d); // wrap sur [-0.5, 0.5]
    const float arg = d / std::max(fraction, 0.05f) * kPi;
    return std::cos(std::clamp(arg, -kPi, kPi));
}

// Direction VERS l'astre : lever à l'est (+x), coucher à l'ouest (-x), arc
// incliné vers -z pour que la trajectoire ne passe pas pile au zénith.
gfx::Vec3 bodyDir(float t, float centre, float fraction, float maxElev, float elev01)
{
    float d = t - centre;
    d -= std::round(d);
    const float sp = std::clamp(d / std::max(fraction, 0.05f) + 0.5f, -0.2f, 1.2f);
    const float az = sp * kPi;
    const float el = elev01 * maxElev;
    return gfx::normalize({std::cos(az) * std::cos(el), std::sin(el), -0.45f * std::cos(el)});
}

// Courbe de couleur du soleil selon son élévation : golden hour -> blanc.
gfx::Vec3 sunCurve(float e)
{
    const gfx::Vec3 dawn{1.35f, 0.55f, 0.25f};
    const gfx::Vec3 mid{1.15f, 0.95f, 0.70f};
    const gfx::Vec3 noon{1.25f, 1.18f, 1.02f};
    if (e <= 0.0f)
        return {0, 0, 0};
    if (e < 0.25f)
        return lerp3(dawn, mid, e / 0.25f);
    return lerp3(mid, noon, (e - 0.25f) / 0.75f);
}

} // namespace

Profile profileFor(const std::string &season, const std::string &weather)
{
    Profile p = seasonBase(season);
    applyWeather(p, weather);
    return p;
}

void EnvironmentState::setSeason(const std::string &season, const std::string &weather)
{
    if (season == _season && weather == _weather)
        return;
    _season = season;
    _weather = weather;
    _tgt = profileFor(season, weather);
}

void EnvironmentState::update(float dt)
{
    // Fondu du profil courant vers la cible (indépendant du framerate).
    const float k = 1.0f - std::exp(-dt / kBlendTau);
    lerpProfile(_cur, _tgt, k);

    _timeOfDay += dt * _timeScale / kDayLengthSeconds;
    _timeOfDay -= std::floor(_timeOfDay);

    // Éclairs d'orage : impulsion qui décroît vite, cadence pseudo-aléatoire.
    if (_weather == "storm")
    {
        _nextFlashIn -= dt;
        if (_nextFlashIn <= 0.0f)
        {
            _flash = 1.0f;
            _nextFlashIn = 3.5f + 3.0f * std::fmod(_timeOfDay * 97.0f, 1.0f);
        }
    }
    _flash = std::max(0.0f, _flash - dt * 5.0f);

    // --- Astres : soleil centré sur t=0.5, lune en opposition (t=0). ---
    const float nightFraction = 1.0f - _cur.dayFraction;
    const float se = elevation01(_timeOfDay, 0.5f, _cur.dayFraction);
    const float me = elevation01(_timeOfDay, 0.0f, nightFraction);
    const gfx::Vec3 toSun = bodyDir(_timeOfDay, 0.5f, _cur.dayFraction, kMaxSunElevation * _cur.sunArcHeight, se);
    const gfx::Vec3 toMoon = bodyDir(_timeOfDay, 0.0f, nightFraction, 0.9f, me);

    Snapshot &s = _snap;
    s.timeOfDay = _timeOfDay;
    s.sunDir = gfx::scale(toSun, -1.0f);
    s.moonDir = gfx::scale(toMoon, -1.0f);
    s.sunVisibility = smooth01(-0.04f, 0.10f, se);
    s.moonVisibility = smooth01(-0.04f, 0.10f, me);
    s.activeLightDir = s.sunVisibility >= s.moonVisibility ? s.sunDir : s.moonDir;

    const gfx::Vec3 sunContrib = gfx::scale(mul3(sunCurve(std::max(se, 0.0f)), _cur.sunTint), s.sunVisibility);
    const gfx::Vec3 moonContrib = gfx::scale({0.20f, 0.26f, 0.46f}, s.moonVisibility);
    s.sunColor = gfx::add(sunContrib, moonContrib);
    s.ambient = mul3(lerp3({0.10f, 0.12f, 0.22f}, {0.42f, 0.44f, 0.52f}, s.sunVisibility), _cur.ambientTint);

    s.starIntensity = 0.08f + 0.92f * (1.0f - s.sunVisibility);
    s.auroraIntensity = _cur.auroraMax * (1.0f - s.sunVisibility);
    s.lightningFlash = _flash;

    // Le ciel s'éclaircit (chaud à l'horizon) quand le soleil est levé.
    s.skyHorizon = gfx::add(_cur.skyHorizon, gfx::scale({0.30f, 0.18f, 0.08f}, s.sunVisibility));
    s.skyZenith = gfx::add(_cur.skyZenith, gfx::scale({0.06f, 0.08f, 0.12f}, s.sunVisibility));
    s.nebulaTint = _cur.nebulaTint;

    s.fogColor = gfx::scale(_cur.fogColor, 0.35f + 0.65f * s.sunVisibility);
    s.fogDensity = _cur.fogDensity;
    s.gradeLift = _cur.gradeLift;
    s.gradeGain = _cur.gradeGain;
    s.heatDistort = _cur.heatDistort;
    s.groundOverlay = _cur.groundOverlay;
    s.groundMix = _cur.groundMix;

    s.particles = _cur.particles;
    s.particles.fireflies *= 1.0f - s.sunVisibility; // lucioles la nuit seulement
}

} // namespace env
```

Ajouter aussi `environment.cpp` aux sources de `zappy_gui` dans `gui/CMakeLists.txt` :

```cmake
add_executable(zappy_gui
    main.cpp
    interface.cpp
    raylibWrapper.cpp
    gameMap.cpp
    netClient.cpp
    protocolParser.cpp
    environment.cpp)
```

- [ ] **Step 5: Vérifier que les tests passent**

Run: `cmake --build build --target zappy_gui_environment_tests && ctest --test-dir build -R gui_environment --output-on-failure`
Expected: `1/1 Test #N: gui_environment .... Passed` avec `all tests passed`.

- [ ] **Step 6: Vérifier que zappy_gui compile toujours**

Run: `make zappy_gui 2>&1 | tail -3`
Expected: build OK, binaire `./zappy_gui` produit.

- [ ] **Step 7: Commit**

```bash
git add gui/environment.hpp gui/environment.cpp gui/tests/test_environment.cpp gui/CMakeLists.txt
git commit -m "feat(gui): EnvironmentState — profils saison/météo interpolés + cycle jour/nuit"
```

---

### Task 2: Lumière dynamique, fog, sol shadé + touches debug

**Files:**
- Modify: `gui/assets/shaders/lighting.fs` (constantes → uniforms, rim, fog)
- Create: `gui/assets/shaders/floor.vs`, `gui/assets/shaders/floor.fs`
- Modify: `gui/raylibWrapper.hpp/.cpp` (`setSceneLighting`, `loadFloorShader`, `beginFloorShading`/`endFloorShading`)
- Modify: `gui/gfxTypes.hpp` (Key::F5/F6/F7)
- Modify: `gui/interface.hpp/.cpp` (membre `_env`, wiring update/render, touches debug)

**Interfaces:**
- Consumes: `env::EnvironmentState` (Task 1) : `_env.setSeason(...)`, `_env.update(dt)`, `_env.snap()`.
- Produces: `RaylibEngine::setSceneLighting(gfx::Vec3 lightDir, gfx::Vec3 lightColor, gfx::Vec3 ambient, gfx::Vec3 fogColor, float fogDensity)` ; `bool RaylibEngine::loadFloorShader(const std::string &vs, const std::string &fs)` ; `void RaylibEngine::beginFloorShading()` / `void RaylibEngine::endFloorShading()` ; `void RaylibEngine::setGroundSeason(gfx::Vec3 overlay, float mix)` ; membres Interface `env::EnvironmentState _env; int _forceSeason{-1}; int _forceWeather{-1};` et helpers `const char *kDebugSeasons[4]`, `const char *kDebugWeathers[6]` (utilisés par les tâches suivantes).

- [ ] **Step 1: lighting.fs — uniforms + rim + fog**

Remplacer intégralement `gui/assets/shaders/lighting.fs` :

```glsl
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
```

- [ ] **Step 2: floor.vs / floor.fs**

Le sol batché (`drawCheckerFloor`, quads rlgl) passe par le shader par défaut,
donc ignore lumière et fog. Le batch rlgl fournit des positions déjà en espace
monde et `matModel` n'est pas garanti — d'où un VS dédié qui prend
`vertexPosition` comme position monde.

Créer `gui/assets/shaders/floor.vs` :

```glsl
#version 330

// VS du sol batché rlgl : les positions du batch sont déjà en espace monde
// (aucune matrice modèle par quad), on les passe telles quelles au FS.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;

out vec3 fragPosition;
out vec2 fragTexCoord;
out vec3 fragNormal;
out vec4 fragColor;

void main()
{
    fragPosition = vertexPosition;
    fragTexCoord = vertexTexCoord;
    fragNormal = vertexNormal;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
```

Créer `gui/assets/shaders/floor.fs` :

```glsl
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
```

- [ ] **Step 3: RaylibEngine — setSceneLighting + floor shader**

`gui/raylibWrapper.hpp`, dans la section « Scene shading / post FX », ajouter :

```cpp
    // Uniforms d'éclairage/fog, à pousser une fois par frame (avant beginMode3D)
    // sur les shaders lighting / instancing / floor. No-op si rien n'est chargé.
    void setSceneLighting(gfx::Vec3 lightDir, gfx::Vec3 lightColor, gfx::Vec3 ambient, gfx::Vec3 fogColor,
                          float fogDensity);
    // Teinte saisonnière du sol (floor shader uniquement).
    void setGroundSeason(gfx::Vec3 overlay, float mix);
    // Shader dédié au sol batché (drawCheckerFloor l'active tout seul ; le sol
    // torique immédiat se bracket via begin/endFloorShading).
    bool loadFloorShader(const std::string &vs, const std::string &fs);
    void beginFloorShading();
    void endFloorShading();
```

`gui/raylibWrapper.cpp` :

1. AVANT la définition de `struct RaylibEngine::Impl` (~ligne 215), déclarer le type au niveau fichier — `Impl` est un type privé, donc les helpers du namespace anonyme ne pourraient pas nommer un type imbriqué dedans :

```cpp
namespace
{
// Locations des uniforms scène partagés par les shaders light / inst / floor.
struct SceneLocs
{
    int sunDir{-1}, sunColor{-1}, ambient{-1}, fogColor{-1}, fogDensity{-1};
};
} // namespace
```

Puis dans `Impl`, ajouter :

```cpp
    SceneLocs lightScene{}, instScene{}, floorScene{};
    Shader floorShader{};
    bool floorLoaded{false};
    int floorViewPosLoc{-1};
    int floorOverlayLoc{-1}, floorMixLoc{-1};
```

2. Helper anonyme (au-dessus de `loadLightingShader`) :

```cpp
namespace
{
SceneLocs cacheSceneLocs(Shader s)
{
    SceneLocs l;
    l.sunDir = GetShaderLocation(s, "sunDir");
    l.sunColor = GetShaderLocation(s, "sunColor");
    l.ambient = GetShaderLocation(s, "ambientColor");
    l.fogColor = GetShaderLocation(s, "fogColor");
    l.fogDensity = GetShaderLocation(s, "fogDensity");
    return l;
}

void applySceneUniforms(Shader s, const SceneLocs &l, Vector3 dir, Vector3 col, Vector3 amb,
                        Vector3 fog, float fogD)
{
    if (l.sunDir >= 0)
        SetShaderValue(s, l.sunDir, &dir, SHADER_UNIFORM_VEC3);
    if (l.sunColor >= 0)
        SetShaderValue(s, l.sunColor, &col, SHADER_UNIFORM_VEC3);
    if (l.ambient >= 0)
        SetShaderValue(s, l.ambient, &amb, SHADER_UNIFORM_VEC3);
    if (l.fogColor >= 0)
        SetShaderValue(s, l.fogColor, &fog, SHADER_UNIFORM_VEC3);
    if (l.fogDensity >= 0)
        SetShaderValue(s, l.fogDensity, &fogD, SHADER_UNIFORM_FLOAT);
}
} // namespace
```

3. À la fin de `loadLightingShader` (avant le `return true`) : `_impl->lightScene = cacheSceneLocs(s);` puis poser les défauts équivalents aux anciennes constantes pour ne jamais rendre noir si `setSceneLighting` n'est pas appelé :

```cpp
    _impl->lightScene = cacheSceneLocs(s);
    applySceneUniforms(s, _impl->lightScene, Vector3{-0.30f, -0.86f, -0.39f}, Vector3{1.00f, 0.96f, 0.88f},
                       Vector3{0.42f, 0.44f, 0.52f}, Vector3{0, 0, 0}, 0.0f);
```

Idem dans `loadInstancingShader` avec `_impl->instScene`.

4. Nouvelles méthodes (après `loadInstancingShader`) :

```cpp
bool RaylibEngine::loadFloorShader(const std::string &vs, const std::string &fs)
{
    if (!FileExists(vs.c_str()) || !FileExists(fs.c_str()))
    {
        TraceLog(LOG_WARNING, "loadFloorShader: missing shader files — flat floor kept");
        return false;
    }
    Shader s = LoadShader(vs.c_str(), fs.c_str());
    if (s.id == 0)
    {
        TraceLog(LOG_WARNING, "loadFloorShader: compile failed — flat floor kept");
        return false;
    }
    _impl->floorShader = s;
    _impl->floorLoaded = true;
    _impl->floorViewPosLoc = GetShaderLocation(s, "viewPos");
    _impl->floorScene = cacheSceneLocs(s);
    _impl->floorOverlayLoc = GetShaderLocation(s, "groundOverlay");
    _impl->floorMixLoc = GetShaderLocation(s, "groundMix");
    applySceneUniforms(s, _impl->floorScene, Vector3{-0.30f, -0.86f, -0.39f}, Vector3{1.00f, 0.96f, 0.88f},
                       Vector3{0.42f, 0.44f, 0.52f}, Vector3{0, 0, 0}, 0.0f);
    setGroundSeason({1, 1, 1}, 0.0f);
    TraceLog(LOG_INFO, "loadFloorShader: '%s' ready", fs.c_str());
    return true;
}

void RaylibEngine::setSceneLighting(gfx::Vec3 lightDir, gfx::Vec3 lightColor, gfx::Vec3 ambient, gfx::Vec3 fogColor,
                                    float fogDensity)
{
    const Vector3 d = toRl(lightDir), c = toRl(lightColor), a = toRl(ambient), f = toRl(fogColor);
    if (_impl->lightLoaded)
        applySceneUniforms(_impl->lightShader, _impl->lightScene, d, c, a, f, fogDensity);
    if (_impl->instLoaded)
        applySceneUniforms(_impl->instShader, _impl->instScene, d, c, a, f, fogDensity);
    if (_impl->floorLoaded)
        applySceneUniforms(_impl->floorShader, _impl->floorScene, d, c, a, f, fogDensity);
}

void RaylibEngine::setGroundSeason(gfx::Vec3 overlay, float mix)
{
    if (!_impl->floorLoaded)
        return;
    const Vector3 o = toRl(overlay);
    if (_impl->floorOverlayLoc >= 0)
        SetShaderValue(_impl->floorShader, _impl->floorOverlayLoc, &o, SHADER_UNIFORM_VEC3);
    if (_impl->floorMixLoc >= 0)
        SetShaderValue(_impl->floorShader, _impl->floorMixLoc, &mix, SHADER_UNIFORM_FLOAT);
}

void RaylibEngine::beginFloorShading()
{
    if (_impl->floorLoaded)
        BeginShaderMode(_impl->floorShader);
}

void RaylibEngine::endFloorShading()
{
    if (_impl->floorLoaded)
        EndShaderMode();
}
```

Note : il existe déjà un `toRl(gfx::Vec3)` dans raylibWrapper.cpp (utilisé par
`drawModelEx`) ; réutiliser. Si `toRl` pour Vec3→Vector3 n'existe pas sous ce
nom, utiliser la fonction de conversion existante du fichier (chercher
`Vector3 toRl`).

5. Dans `beginMode3D`, après les deux `SetShaderValue` viewPos existants, ajouter le viewPos du floor :

```cpp
    if (_impl->floorLoaded && _impl->floorViewPosLoc >= 0)
        SetShaderValue(_impl->floorShader, _impl->floorViewPosLoc, &eye, SHADER_UNIFORM_VEC3);
```

6. Dans `drawCheckerFloor`, bracketer les deux batches :

```cpp
void RaylibEngine::drawCheckerFloor(gfx::TextureHandle dark, gfx::TextureHandle light, int w, int h, float tileSize,
                                    float quadSize, float surfaceY)
{
    beginFloorShading();
    if (_impl->valid(dark, _impl->textures.size()))
        drawFloorBatch(_impl->textures[dark], true, w, h, tileSize, quadSize, surfaceY);
    if (_impl->valid(light, _impl->textures.size()))
        drawFloorBatch(_impl->textures[light], false, w, h, tileSize, quadSize, surfaceY);
    endFloorShading();
}
```

7. Dans le destructeur, libérer : `if (_impl->floorLoaded) UnloadShader(_impl->floorShader);`

- [ ] **Step 4: Touches F5/F6/F7 dans gfxTypes + mapping raylib**

`gui/gfxTypes.hpp` : ajouter `F5, F6, F7,` à la fin de `enum class Key` (après `F3,`).
`gui/raylibWrapper.cpp` : dans `toRlKey` (ligne ~53), ajouter les cases `case gfx::Key::F5: return KEY_F5;` etc. (suivre le style du switch existant).

- [ ] **Step 5: Interface — wiring env + debug + HUD**

`gui/interface.hpp` :
- Ajouter `#include "environment.hpp"` après `#include "gameMap.hpp"`.
- Dans les membres (près de `_weatherVisible`) :

```cpp
    // --- Environnement (saison / météo / jour-nuit) ---
    env::EnvironmentState _env;   // source unique des paramètres visuels ambiants
    int _forceSeason{-1};         // F5: index dans kDebugSeasons, -1 = serveur
    int _forceWeather{-1};        // F6: index dans kDebugWeathers, -1 = serveur
```

`gui/interface.cpp` :
- En haut (zone des constantes, près de `kPi`) :

```cpp
constexpr const char *kDebugSeasons[] = {"spring", "summer", "autumn", "winter"};
constexpr const char *kDebugWeathers[] = {"clear", "rain", "storm", "fog", "heat", "fertile"};
```

- Constructeur : après `_engine.enableBloom(...)` (ligne ~24) :

```cpp
    _engine.loadFloorShader("assets/shaders/floor.vs", "assets/shaders/floor.fs");
```

- `handleInput()` : après le bloc `V` (ligne ~982) :

```cpp
    // ---- Debug environnement : forcer saison / météo, accélérer l'horloge.
    if (_engine.keyPressed(gfx::Key::F5))
        _forceSeason = _forceSeason >= 3 ? -1 : _forceSeason + 1;
    if (_engine.keyPressed(gfx::Key::F6))
        _forceWeather = _forceWeather >= 5 ? -1 : _forceWeather + 1;
    if (_engine.keyPressed(gfx::Key::F7))
        _env.setTimeScale(_env.timeScale() > 1.0f ? 1.0f : 40.0f);
```

- `update()` : localiser l'endroit où `_elapsed` est incrémenté (chercher `_elapsed +=`) et ajouter juste après :

```cpp
    const std::string envSeason = _forceSeason >= 0 ? kDebugSeasons[_forceSeason] : _state.season;
    const std::string envWeather = _forceWeather >= 0 ? kDebugWeathers[_forceWeather] : _state.weather;
    _env.setSeason(envSeason, envWeather);
    _env.update(dt); // dt = la même valeur que celle ajoutée à _elapsed
```

- `render()` : juste avant `_engine.beginMode3D(_camera);` (ligne ~1376) :

```cpp
    const env::Snapshot &es = _env.snap();
    // Flash d'éclair : boost bref de la lumière 3D entière.
    const gfx::Vec3 flashAdd = gfx::scale({1.8f, 1.8f, 2.2f}, es.lightningFlash);
    _engine.setSceneLighting(es.activeLightDir, gfx::add(es.sunColor, flashAdd),
                             gfx::add(es.ambient, gfx::scale(flashAdd, 0.3f)), es.fogColor,
                             _weatherVisible ? es.fogDensity : 0.0f);
    _engine.setGroundSeason(es.groundOverlay, _weatherVisible ? es.groundMix : 0.0f);
```

- HUD (`drawHud`, ligne ~2229) : remplacer la ligne Season/Weather par une version qui montre l'heure et les forçages :

```cpp
    const float hours = _env.snap().timeOfDay * 24.0f;
    _engine.drawText(gfx::fmt("Season: %s%s   Weather: %s%s   %02dh%02d   FX: %s", seasonLabel(_state.season),
                              _forceSeason >= 0 ? "*" : "", weatherLabel(_state.weather),
                              _forceWeather >= 0 ? "*" : "", static_cast<int>(hours),
                              static_cast<int>(std::fmod(hours, 1.0f) * 60.0f), _weatherVisible ? "on" : "off"),
                     10, 52, 16, seasonColor(_state.season));
```

(Le `*` marque une valeur forcée par F5/F6 — l'étiquette continue d'afficher l'état serveur.)

- [ ] **Step 6: Build + vérification visuelle**

Run: `make zappy_gui 2>&1 | tail -3` — Expected: OK.
Run: `ctest --test-dir build -R gui_environment --output-on-failure` — Expected: PASS.
Puis lancer serveur + GUI (commande des Global Constraints) et vérifier :
- Le sol et les modèles changent de teinte quand on presse F5 (4 saisons) — transitions fondues ~4 s.
- F7 : l'heure du HUD défile vite ; la scène passe nuit (bleu sombre) → aube dorée → jour → crépuscule. La nuit, les modèles restent lisibles (clair de lune bleuté).
- F6 storm : scène très sombre + flashs périodiques. F6 fog : les tuiles lointaines se noient dans le gris.
- V coupe fog/teinte saison (lumière jour/nuit conservée).

- [ ] **Step 7: Commit**

```bash
git add gui/assets/shaders/lighting.fs gui/assets/shaders/floor.vs gui/assets/shaders/floor.fs \
        gui/raylibWrapper.hpp gui/raylibWrapper.cpp gui/gfxTypes.hpp gui/interface.hpp gui/interface.cpp
git commit -m "feat(gui): lumière dynamique jour/nuit + fog + sol shadé + touches debug F5-F7"
```

---

### Task 3: Ciel procédural + billboards Luan (soleil) / Palasse (lune)

**Files:**
- Create: `gui/assets/shaders/sky_procedural.fs`
- Create: `gui/assets/shaders/celestial.fs`
- Modify: `gui/raylibWrapper.hpp/.cpp` (`loadProceduralSky`, `setSkyParams`, `loadCelestialShader`, `drawCelestial`)
- Modify: `gui/interface.hpp/.cpp` (chargement, draw dans render())

**Interfaces:**
- Consumes: `env::Snapshot` (sunDir/moonDir/visibilities/sky*/star/aurora/lightning), skybox.vs existant, `drawSkybox()` existant.
- Produces: `bool RaylibEngine::loadProceduralSky(const std::string &vs, const std::string &fs)` (remplace la texture du skybox par un shader pur — `drawSkybox()` inchangé) ; `void RaylibEngine::setSkyParams(float time, gfx::Vec3 toSun, gfx::Vec3 toMoon, gfx::Vec3 sunGlowColor, gfx::Vec3 horizon, gfx::Vec3 zenith, gfx::Vec3 nebula, float stars, float aurora, float lightning)` ; `bool RaylibEngine::loadCelestialShader(const std::string &fs)` ; `void RaylibEngine::drawCelestial(gfx::TextureHandle tex, const gfx::Camera &cam, gfx::Vec3 toBody, float angularSizeDeg, gfx::Vec3 emissive, float alpha, bool circleMask)` ; membres Interface `_sunTexture`, `_moonTexture`.

- [ ] **Step 1: sky_procedural.fs**

Créer `gui/assets/shaders/sky_procedural.fs` :

```glsl
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
```

- [ ] **Step 2: celestial.fs**

Créer `gui/assets/shaders/celestial.fs` :

```glsl
#version 330

// Billboard d'astre (Luan = soleil, Palasse = lune). emissive est HDR pour
// nourrir le bloom ; circleMask découpe les textures à fond opaque (palasse).

in vec2 fragTexCoord;
in vec4 fragColor;

uniform sampler2D texture0;
uniform vec3 emissive;
uniform float bodyAlpha;
uniform int circleMask;

out vec4 finalColor;

void main()
{
    vec4 t = texture(texture0, fragTexCoord);
    float a = t.a;
    if (circleMask == 1)
    {
        float d = length(fragTexCoord - vec2(0.5));
        a *= 1.0 - smoothstep(0.44, 0.5, d);
    }
    a *= bodyAlpha;
    if (a < 0.01)
        discard;
    finalColor = vec4(t.rgb * emissive, a);
}
```

- [ ] **Step 3: RaylibEngine — loadProceduralSky / setSkyParams / drawCelestial**

`gui/raylibWrapper.hpp`, section « Composite scene primitives » :

```cpp
    // Ciel procédural : mêmes géométrie/draw que loadSkybox, mais 100% shader.
    bool loadProceduralSky(const std::string &vs, const std::string &fs);
    void setSkyParams(float time, gfx::Vec3 toSun, gfx::Vec3 toMoon, gfx::Vec3 sunGlowColor, gfx::Vec3 horizon,
                      gfx::Vec3 zenith, gfx::Vec3 nebula, float stars, float aurora, float lightning);
    // Billboards soleil/lune, à dessiner juste après drawSkybox (depth off).
    bool loadCelestialShader(const std::string &fs);
    void drawCelestial(gfx::TextureHandle tex, const gfx::Camera &cam, gfx::Vec3 toBody, float angularSizeDeg,
                       gfx::Vec3 emissive, float alpha, bool circleMask);
```

`gui/raylibWrapper.cpp` :

1. `Impl` — ajouter :

```cpp
    // Ciel procédural : locations des uniforms de sky_procedural.fs.
    struct SkyLocs
    {
        int time{-1}, toSun{-1}, toMoon{-1}, sunGlow{-1}, horizon{-1}, zenith{-1}, nebula{-1}, stars{-1}, aurora{-1},
            lightning{-1};
    };
    SkyLocs skyLocs{};
    bool skyProcedural{false};

    Shader celestialShader{};
    bool celestialLoaded{false};
    int celEmissiveLoc{-1}, celAlphaLoc{-1}, celMaskLoc{-1};
```

2. Implémentations (placer après `unloadSkybox`) :

```cpp
bool RaylibEngine::loadProceduralSky(const std::string &vs, const std::string &fs)
{
    if (!FileExists(vs.c_str()) || !FileExists(fs.c_str()))
    {
        TraceLog(LOG_WARNING, "loadProceduralSky: missing shader files");
        return false;
    }
    Shader s = LoadShader(vs.c_str(), fs.c_str());
    if (s.id == 0)
    {
        TraceLog(LOG_WARNING, "loadProceduralSky: compile failed");
        return false;
    }
    unloadSkybox(); // remplace un éventuel skybox texturé déjà chargé
    _impl->skyShader = s;
    _impl->skybox = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    _impl->skybox.materials[0].shader = s;
    _impl->skyLocs.time = GetShaderLocation(s, "time");
    _impl->skyLocs.toSun = GetShaderLocation(s, "toSun");
    _impl->skyLocs.toMoon = GetShaderLocation(s, "toMoon");
    _impl->skyLocs.sunGlow = GetShaderLocation(s, "sunGlowColor");
    _impl->skyLocs.horizon = GetShaderLocation(s, "horizonColor");
    _impl->skyLocs.zenith = GetShaderLocation(s, "zenithColor");
    _impl->skyLocs.nebula = GetShaderLocation(s, "nebulaTint");
    _impl->skyLocs.stars = GetShaderLocation(s, "starIntensity");
    _impl->skyLocs.aurora = GetShaderLocation(s, "auroraIntensity");
    _impl->skyLocs.lightning = GetShaderLocation(s, "lightning");
    _impl->skyboxLoaded = true;
    _impl->skyProcedural = true;
    TraceLog(LOG_INFO, "loadProceduralSky: '%s' ready", fs.c_str());
    return true;
}

void RaylibEngine::setSkyParams(float time, gfx::Vec3 toSun, gfx::Vec3 toMoon, gfx::Vec3 sunGlowColor,
                                gfx::Vec3 horizon, gfx::Vec3 zenith, gfx::Vec3 nebula, float stars, float aurora,
                                float lightning)
{
    if (!_impl->skyProcedural)
        return;
    const Shader &s = _impl->skyShader;
    const Vector3 ts = toRl(toSun), tm = toRl(toMoon), sg = toRl(sunGlowColor), ho = toRl(horizon), ze = toRl(zenith),
                  ne = toRl(nebula);
    const auto &l = _impl->skyLocs;
    if (l.time >= 0)
        SetShaderValue(s, l.time, &time, SHADER_UNIFORM_FLOAT);
    if (l.toSun >= 0)
        SetShaderValue(s, l.toSun, &ts, SHADER_UNIFORM_VEC3);
    if (l.toMoon >= 0)
        SetShaderValue(s, l.toMoon, &tm, SHADER_UNIFORM_VEC3);
    if (l.sunGlow >= 0)
        SetShaderValue(s, l.sunGlow, &sg, SHADER_UNIFORM_VEC3);
    if (l.horizon >= 0)
        SetShaderValue(s, l.horizon, &ho, SHADER_UNIFORM_VEC3);
    if (l.zenith >= 0)
        SetShaderValue(s, l.zenith, &ze, SHADER_UNIFORM_VEC3);
    if (l.nebula >= 0)
        SetShaderValue(s, l.nebula, &ne, SHADER_UNIFORM_VEC3);
    if (l.stars >= 0)
        SetShaderValue(s, l.stars, &stars, SHADER_UNIFORM_FLOAT);
    if (l.aurora >= 0)
        SetShaderValue(s, l.aurora, &aurora, SHADER_UNIFORM_FLOAT);
    if (l.lightning >= 0)
        SetShaderValue(s, l.lightning, &lightning, SHADER_UNIFORM_FLOAT);
}

bool RaylibEngine::loadCelestialShader(const std::string &fs)
{
    if (!FileExists(fs.c_str()))
    {
        TraceLog(LOG_WARNING, "loadCelestialShader: missing '%s'", fs.c_str());
        return false;
    }
    Shader s = LoadShader(nullptr, fs.c_str());
    if (s.id == 0)
    {
        TraceLog(LOG_WARNING, "loadCelestialShader: compile failed");
        return false;
    }
    _impl->celestialShader = s;
    _impl->celEmissiveLoc = GetShaderLocation(s, "emissive");
    _impl->celAlphaLoc = GetShaderLocation(s, "bodyAlpha");
    _impl->celMaskLoc = GetShaderLocation(s, "circleMask");
    _impl->celestialLoaded = true;
    TraceLog(LOG_INFO, "loadCelestialShader: ready");
    return true;
}

void RaylibEngine::drawCelestial(gfx::TextureHandle tex, const gfx::Camera &cam, gfx::Vec3 toBody,
                                 float angularSizeDeg, gfx::Vec3 emissive, float alpha, bool circleMask)
{
    if (!_impl->celestialLoaded || !_impl->valid(tex, _impl->textures.size()) || alpha <= 0.01f)
        return;

    // Quad à distance fixe (dans le far plane), taille depuis l'angle apparent.
    const float dist = 800.0f;
    const float half = dist * std::tan(angularSizeDeg * 0.5f * DEG2RAD);
    const gfx::Vec3 dir = gfx::normalize(toBody);
    const gfx::Vec3 centre = gfx::add(cam.position, gfx::scale(dir, dist));
    gfx::Vec3 right = gfx::normalize({-dir.z, 0.0f, dir.x}); // horizontal, perpendiculaire à dir
    if (gfx::length(right) < 0.5f)
        right = {1, 0, 0};
    const gfx::Vec3 up = gfx::normalize(gfx::Vec3{dir.y * right.z - dir.z * right.y, dir.z * right.x - dir.x * right.z,
                                                  dir.x * right.y - dir.y * right.x});
    const gfx::Vec3 a = gfx::add(gfx::add(centre, gfx::scale(right, -half)), gfx::scale(up, half));
    const gfx::Vec3 b = gfx::add(gfx::add(centre, gfx::scale(right, -half)), gfx::scale(up, -half));
    const gfx::Vec3 c = gfx::add(gfx::add(centre, gfx::scale(right, half)), gfx::scale(up, -half));
    const gfx::Vec3 d = gfx::add(gfx::add(centre, gfx::scale(right, half)), gfx::scale(up, half));

    const Vector3 em = toRl(emissive);
    const int mask = circleMask ? 1 : 0;
    if (_impl->celEmissiveLoc >= 0)
        SetShaderValue(_impl->celestialShader, _impl->celEmissiveLoc, &em, SHADER_UNIFORM_VEC3);
    if (_impl->celAlphaLoc >= 0)
        SetShaderValue(_impl->celestialShader, _impl->celAlphaLoc, &alpha, SHADER_UNIFORM_FLOAT);
    if (_impl->celMaskLoc >= 0)
        SetShaderValue(_impl->celestialShader, _impl->celMaskLoc, &mask, SHADER_UNIFORM_INT);

    // Derrière tout : depth off, comme le skybox.
    rlDisableDepthMask();
    rlDisableDepthTest();
    BeginShaderMode(_impl->celestialShader);
    Texture2D &t = _impl->textures[tex];
    rlSetTexture(t.id);
    rlBegin(RL_QUADS);
    rlColor4ub(255, 255, 255, 255);
    rlNormal3f(-dir.x, -dir.y, -dir.z);
    rlTexCoord2f(0.0f, 0.0f);
    rlVertex3f(a.x, a.y, a.z);
    rlTexCoord2f(0.0f, 1.0f);
    rlVertex3f(b.x, b.y, b.z);
    rlTexCoord2f(1.0f, 1.0f);
    rlVertex3f(c.x, c.y, c.z);
    rlTexCoord2f(1.0f, 0.0f);
    rlVertex3f(d.x, d.y, d.z);
    rlEnd();
    rlSetTexture(0);
    EndShaderMode();
    rlEnableDepthTest();
    rlEnableDepthMask();
}
```

Destructeur : `if (_impl->celestialLoaded) UnloadShader(_impl->celestialShader);`.
Note : `unloadSkybox()` gère déjà le shader sky + cube ; avec `skyProcedural`, `skyboxTex.id == 0` donc rien de plus à libérer.

- [ ] **Step 4: Interface — chargement + draw**

`gui/interface.hpp`, membres (près de `_grassTileTexture`) :

```cpp
    gfx::TextureHandle _sunTexture{gfx::NoHandle};  // Luan, notre astre du jour
    gfx::TextureHandle _moonTexture{gfx::NoHandle}; // Palasse, veilleur de nuit
```

`gui/interface.cpp` constructeur — remplacer la ligne `_engine.loadSkybox(...)` (ligne ~18) par :

```cpp
    // Ciel procédural (jour/nuit, nébuleuses, aurores) ; si le shader échoue,
    // on retombe sur le panorama statique d'origine.
    if (!_engine.loadProceduralSky("assets/shaders/skybox.vs", "assets/shaders/sky_procedural.fs"))
        _engine.loadSkybox("assets/Background.png", "assets/shaders/skybox.vs", "assets/shaders/skybox.fs");
    _engine.loadCelestialShader("assets/shaders/celestial.fs");
    _sunTexture = _engine.loadTexture("assets/luan.png");
    _moonTexture = _engine.loadTexture("assets/palasse.png");
    if (_sunTexture != gfx::NoHandle)
        _engine.setTextureBilinear(_sunTexture);
    if (_moonTexture != gfx::NoHandle)
        _engine.setTextureBilinear(_moonTexture);
```

`render()` — remplacer le bloc skybox (ligne ~1378) :

```cpp
    // 360 background d'abord, pour que la scène se dessine devant.
    const gfx::Vec3 toSun = gfx::scale(es.sunDir, -1.0f);
    const gfx::Vec3 toMoon = gfx::scale(es.moonDir, -1.0f);
    _engine.setSkyParams(_elapsed, toSun, toMoon, es.sunColor, es.skyHorizon, es.skyZenith, es.nebulaTint,
                         es.starIntensity, _weatherVisible ? es.auroraIntensity : 0.0f, es.lightningFlash);
    _engine.drawSkybox();
    // Astres : la lune d'abord, le soleil par-dessus en cas de chevauchement.
    _engine.drawCelestial(_moonTexture, _camera, toMoon, 7.0f, {1.25f, 1.35f, 1.6f}, es.moonVisibility, true);
    _engine.drawCelestial(_sunTexture, _camera, toSun, 9.0f, {2.4f, 2.1f, 1.5f}, es.sunVisibility, false);
```

- [ ] **Step 5: Build + vérification visuelle**

Run: `make zappy_gui 2>&1 | tail -3` — Expected: OK, et au lancement les logs montrent `loadProceduralSky: ... ready`.
Vérifier (F7 pour accélérer le cycle) :
- Nuit : étoiles qui scintillent, nébuleuses teintées saison (F5 : violet→or→cuivre→bleu), Palasse visible ronde (fond blanc découpé), halo lunaire.
- Aube : le ciel s'embrase à l'est, Luan se lève avec un halo doré qui bloom.
- Le soleil du ciel correspond à la direction de la lumière sur les modèles (ombres des reliefs vers l'opposé).
- Hiver de nuit : aurores vertes ondulantes. Orage : flashs illuminant le ciel.
- Fallback : renommer temporairement sky_procedural.fs → le GUI relance sur Background.png sans crash, puis restaurer.

- [ ] **Step 6: Commit**

```bash
git add gui/assets/shaders/sky_procedural.fs gui/assets/shaders/celestial.fs \
        gui/raylibWrapper.hpp gui/raylibWrapper.cpp gui/interface.hpp gui/interface.cpp
git commit -m "feat(gui): ciel procédural (étoiles, nébuleuses, aurores) + billboards Luan/Palasse"
```

---

### Task 4: Post-process cinématique (HDR 16F, ACES, god rays, grading, vignette, heat)

**Files:**
- Create: `gui/assets/shaders/post_composite.fs`
- Modify: `gui/raylibWrapper.hpp/.cpp` (RT 16F, `enablePostFx` remplace `enableBloom`, `setPostFxParams`)
- Modify: `gui/interface.cpp` (appel enablePostFx + params par frame)

**Interfaces:**
- Consumes: chaîne bloom existante (`sceneRT`/`bloomRT`, `bloom_extract.fs` conservé tel quel), `env::Snapshot` (gradeLift/gradeGain/heatDistort/sunVisibility), `worldToScreen`.
- Produces: `bool RaylibEngine::enablePostFx(const std::string &extractFs, const std::string &compositeFs)` (l'ancien `enableBloom` est renommé — mettre à jour l'appelant) ; `void RaylibEngine::setPostFxParams(gfx::Vec2 sunScreen01, float godrayStrength, gfx::Vec3 gradeLift, gfx::Vec3 gradeGain, float heat, float time)`.

- [ ] **Step 1: post_composite.fs**

Créer `gui/assets/shaders/post_composite.fs` :

```glsl
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
```

- [ ] **Step 2: RaylibEngine — RT 16F + enablePostFx + setPostFxParams**

`gui/raylibWrapper.hpp` : remplacer la déclaration `enableBloom` par :

```cpp
    // Post FX : scène rendue dans un RT HDR (RGBA16F) ; endMode3D extrait le
    // bloom en demi-res puis composite (ACES, god rays, grading, vignette).
    bool enablePostFx(const std::string &extractFs, const std::string &compositeFs);
    // Paramètres par frame du composite. sunScreen01 en UV écran (0..1, origine
    // en haut à gauche) ; godrayStrength 0 quand le soleil est hors champ.
    void setPostFxParams(gfx::Vec2 sunScreen01, float godrayStrength, gfx::Vec3 gradeLift, gfx::Vec3 gradeGain,
                         float heat, float time);
```

`gui/raylibWrapper.cpp` :

1. `Impl` — étendre le bloc bloom :

```cpp
    // Locations du composite + valeurs par frame (appliquées dans endMode3D).
    struct PostLocs
    {
        int glowTex{-1}, sunScreen{-1}, godray{-1}, lift{-1}, gain{-1}, heat{-1}, time{-1};
    };
    PostLocs postLocs{};
    struct PostParams
    {
        Vector2 sunScreen{0.5f, 0.5f};
        float godray{0};
        Vector3 lift{0, 0, 0};
        Vector3 gain{1, 1, 1};
        float heat{0};
        float time{0};
    };
    PostParams post{};
```

2. Helper de création de RT HDR (namespace anonyme, avant `enablePostFx`) — même schéma que l'exemple raylib « hdr render texture » :

```cpp
namespace
{
// LoadRenderTexture force RGBA8 ; pour un pipeline HDR (valeurs > 1 pour le
// bloom/ACES) on assemble un FBO avec une texture couleur RGBA16F à la main.
RenderTexture2D loadRenderTexture16F(int width, int height)
{
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    if (target.id == 0)
        return target;
    rlEnableFramebuffer(target.id);

    target.texture.id = rlLoadTexture(nullptr, width, height, PIXELFORMAT_UNCOMPRESSED_R16G16B16A16, 1);
    target.texture.width = width;
    target.texture.height = height;
    target.texture.format = PIXELFORMAT_UNCOMPRESSED_R16G16B16A16;
    target.texture.mipmaps = 1;

    target.depth.id = rlLoadTextureDepth(width, height, true);
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19; // DEPTH_COMPONENT_24BIT (valeur raylib interne)
    target.depth.mipmaps = 1;

    rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    if (!rlFramebufferComplete(target.id))
    {
        rlUnloadFramebuffer(target.id);
        target.id = 0;
    }
    rlDisableFramebuffer();
    return target;
}
} // namespace
```

3. Renommer `enableBloom` en `enablePostFx` et remplacer les deux `LoadRenderTexture` par `loadRenderTexture16F` (sceneRT ET bloomRT — le glow doit rester HDR). Si `loadRenderTexture16F` renvoie `id == 0`, retomber sur `LoadRenderTexture` classique avec un `TraceLog(LOG_WARNING, "enablePostFx: 16F unsupported — LDR post FX")`. Après le chargement du shader composite, cacher les locations :

```cpp
    _impl->postLocs.glowTex = GetShaderLocation(comb, "glowTex");
    _impl->postLocs.sunScreen = GetShaderLocation(comb, "sunScreen");
    _impl->postLocs.godray = GetShaderLocation(comb, "godray");
    _impl->postLocs.lift = GetShaderLocation(comb, "gradeLift");
    _impl->postLocs.gain = GetShaderLocation(comb, "gradeGain");
    _impl->postLocs.heat = GetShaderLocation(comb, "heat");
    _impl->postLocs.time = GetShaderLocation(comb, "time");
```

(`bloomGlowLoc` disparaît au profit de `postLocs.glowTex` ; `bloomResLoc` inchangé.)

4. Idem dans `beginMode3D` : le resize des RT utilise `loadRenderTexture16F` (avec le même fallback LDR).

5. `setPostFxParams` — stocke, c'est `endMode3D` qui applique :

```cpp
void RaylibEngine::setPostFxParams(gfx::Vec2 sunScreen01, float godrayStrength, gfx::Vec3 gradeLift,
                                   gfx::Vec3 gradeGain, float heat, float time)
{
    _impl->post.sunScreen = Vector2{sunScreen01.x, sunScreen01.y};
    _impl->post.godray = godrayStrength;
    _impl->post.lift = toRl(gradeLift);
    _impl->post.gain = toRl(gradeGain);
    _impl->post.heat = heat;
    _impl->post.time = time;
}
```

6. Dans `endMode3D`, pass 2 : avant le `DrawTextureRec`, pousser tous les uniforms :

```cpp
        const auto &pl = _impl->postLocs;
        const auto &pp = _impl->post;
        BeginShaderMode(_impl->bloomCombine);
        if (pl.glowTex >= 0)
            SetShaderValueTexture(_impl->bloomCombine, pl.glowTex, _impl->bloomRT.texture);
        if (pl.sunScreen >= 0)
            SetShaderValue(_impl->bloomCombine, pl.sunScreen, &pp.sunScreen, SHADER_UNIFORM_VEC2);
        if (pl.godray >= 0)
            SetShaderValue(_impl->bloomCombine, pl.godray, &pp.godray, SHADER_UNIFORM_FLOAT);
        if (pl.lift >= 0)
            SetShaderValue(_impl->bloomCombine, pl.lift, &pp.lift, SHADER_UNIFORM_VEC3);
        if (pl.gain >= 0)
            SetShaderValue(_impl->bloomCombine, pl.gain, &pp.gain, SHADER_UNIFORM_VEC3);
        if (pl.heat >= 0)
            SetShaderValue(_impl->bloomCombine, pl.heat, &pp.heat, SHADER_UNIFORM_FLOAT);
        if (pl.time >= 0)
            SetShaderValue(_impl->bloomCombine, pl.time, &pp.time, SHADER_UNIFORM_FLOAT);
```

- [ ] **Step 3: Interface — brancher**

`gui/interface.cpp` constructeur : remplacer `_engine.enableBloom("assets/shaders/bloom_extract.fs", "assets/shaders/bloom_combine.fs");` par :

```cpp
    _engine.enablePostFx("assets/shaders/bloom_extract.fs", "assets/shaders/post_composite.fs");
```

`render()` — dans le bloc env ajouté en Task 2 (avant `beginMode3D`), après `setGroundSeason` :

```cpp
    // God rays depuis la position écran du soleil (0 si dos au soleil).
    const gfx::Vec3 toSunW = gfx::scale(es.sunDir, -1.0f);
    const gfx::Vec3 camFwdPre = gfx::normalize(gfx::sub(_camera.target, _camera.position));
    float godray = 0.0f;
    gfx::Vec2 sunScreen01{0.5f, 0.5f};
    const float facing = gfx::dot(camFwdPre, toSunW);
    if (facing > 0.05f && es.sunVisibility > 0.01f)
    {
        const gfx::Vec2 sp = _engine.worldToScreen(_camera, gfx::add(_camera.position, gfx::scale(toSunW, 500.0f)));
        sunScreen01 = {sp.x / static_cast<float>(_engine.screenWidth()),
                       sp.y / static_cast<float>(_engine.screenHeight())};
        godray = es.sunVisibility * std::min(facing * 1.5f, 1.0f) * 0.8f;
    }
    const bool fx = _weatherVisible;
    _engine.setPostFxParams(sunScreen01, fx ? godray : 0.0f, fx ? es.gradeLift : gfx::Vec3{0, 0, 0},
                            fx ? es.gradeGain : gfx::Vec3{1, 1, 1}, fx ? es.heatDistort : 0.0f, _elapsed);
```

(`bloom_combine.fs` reste dans le dépôt, plus référencé — le supprimer.)

```bash
git rm gui/assets/shaders/bloom.fs gui/assets/shaders/bloom_combine.fs
```

(`bloom.fs` était déjà orphelin — vérifier avec `grep -rn "bloom.fs\|bloom_combine" gui/` avant de supprimer.)

- [ ] **Step 4: Build + vérification visuelle**

Run: `make zappy_gui 2>&1 | tail -3` — Expected: OK.
Vérifier :
- L'image globale est tonemappée (hautes lumières roll-off doux, pas de clip brutal blanc).
- Face au soleil levant : rayons radiaux visibles à travers la scène ; dos au soleil : aucun artefact.
- Attention au sens Y de `sunScreen` : si les rays divergent du MAUVAIS point (miroir vertical), inverser `sunScreen01.y = 1.0f - sunScreen01.y` — noter le résultat.
- F6 heat : l'écran ondule vers le haut. F5 saisons : le grading change la tonalité (or été, cuivre automne, froid hiver).
- Vignette visible mais discrète dans les coins ; V remet un rendu neutre (tonemap conservé).

- [ ] **Step 5: Commit**

```bash
git add -A gui/assets/shaders gui/raylibWrapper.hpp gui/raylibWrapper.cpp gui/interface.cpp
git commit -m "feat(gui): pipeline HDR 16F — ACES, god rays, color grading, vignette, heat haze"
```

---

### Task 5: Particules 3D monde (remplace l'overlay 2D)

**Files:**
- Create: `gui/particles.hpp`
- Create: `gui/particles.cpp`
- Modify: `gui/raylibWrapper.hpp/.cpp` (`setAdditiveBlend`, `createRadialTexture`)
- Modify: `gui/interface.hpp/.cpp` (membre `_particles`, update/draw, suppression `drawWeatherOverlay`)
- Modify: `gui/CMakeLists.txt` (ajouter `particles.cpp`)

**Interfaces:**
- Consumes: `env::ParticleProfile` (Task 1), `RaylibEngine` (drawTexturedQuad3D, drawQuad3D, drawLine3D, drawCircle3D), `gfx::Camera`.
- Produces: `class ParticleSystem` avec `void update(float dt, const env::ParticleProfile &profile, gfx::Vec3 camPos, float groundY, bool hasGround, float time)` et `void draw(RaylibEngine &engine, const gfx::Camera &cam, gfx::TextureHandle softDot) const` ; `void RaylibEngine::setAdditiveBlend(bool on)` ; `gfx::TextureHandle RaylibEngine::createRadialTexture(int size)`.

- [ ] **Step 1: RaylibEngine — blend additif + texture radiale**

`gui/raylibWrapper.hpp`, section 3D drawing :

```cpp
    // Blend additif pour les particules émissives (lucioles, braises, spores).
    void setAdditiveBlend(bool on);
    // Petit disque doux généré (dégradé radial blanc -> transparent).
    gfx::TextureHandle createRadialTexture(int size);
```

`gui/raylibWrapper.cpp` :

```cpp
void RaylibEngine::setAdditiveBlend(bool on)
{
    rlDrawRenderBatchActive(); // flush du batch courant avant le changement d'état
    rlSetBlendMode(on ? RL_BLEND_ADDITIVE : RL_BLEND_ALPHA);
}

gfx::TextureHandle RaylibEngine::createRadialTexture(int size)
{
    Image img = GenImageGradientRadial(size, size, 0.0f, WHITE, BLANK);
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    if (t.id == 0)
        return gfx::NoHandle;
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    _impl->textures.push_back(t);
    return static_cast<gfx::TextureHandle>(_impl->textures.size() - 1);
}
```

(Si la version raylib installée nomme la fonction `GenImageGradientRadial` différemment — raylib 5 l'a renommée en `GenImageGradientRadial` est correct pour 4.x ; en 5.0 c'est toujours `GenImageGradientRadial`. Si la compilation échoue, vérifier `grep GradientRadial /usr/include/raylib.h`.)

- [ ] **Step 2: particles.hpp**

Créer `gui/particles.hpp` :

```cpp
#pragma once

// ---------------------------------------------------------------------------
// ParticleSystem — sim CPU + rendu billboard des effets météo/saison 3D
// (neige, feuilles, pétales, lucioles, poussière, braises, pluie, spores,
// éclaboussures). Pool fixe, spawn dans un volume centré caméra pour couvrir
// la vue en mode grille comme en mode tore.
// ---------------------------------------------------------------------------

#include "environment.hpp"
#include "gfxTypes.hpp"
#include <cstdint>
#include <vector>

class RaylibEngine;

class ParticleSystem
{
  public:
    ParticleSystem();

    // groundY: hauteur du sol pour les impacts (mode grille) ;
    // hasGround=false (mode tore) désactive dépôt neige / splash pluie.
    void update(float dt, const env::ParticleProfile &profile, gfx::Vec3 camPos, float groundY, bool hasGround,
                float time);
    void draw(RaylibEngine &engine, const gfx::Camera &cam, gfx::TextureHandle softDot) const;
    int aliveCount() const; // pour l'overlay perf

  private:
    enum Kind : std::uint8_t
    {
        Snow,
        Leaf,
        Petal,
        Firefly,
        Dust,
        Ember,
        Rain,
        Spore,
        Splash,
        KindCount
    };

    struct Particle
    {
        gfx::Vec3 pos{}, vel{};
        float life{0}, maxLife{1}; // life <= 0 == slot libre
        float size{1};
        float rot{0}, rotSpeed{0};
        float phase{0}; // décalage pour flottement / clignotement
        std::uint8_t kind{Snow};
    };

    std::vector<Particle> _pool; // taille fixe, jamais réalloué
    float _spawnAcc[KindCount]{};
    std::uint32_t _rng{0x12345u};

    float frand(); // xorshift 0..1
    Particle *alloc();
    void spawnKind(std::uint8_t kind, float rate, float dt, gfx::Vec3 camPos, float groundY, bool hasGround,
                   const env::ParticleProfile &p);
};
```

- [ ] **Step 3: particles.cpp**

Créer `gui/particles.cpp` :

```cpp
#include "particles.hpp"
#include "raylibWrapper.hpp"
#include <algorithm>
#include <cmath>

namespace
{
constexpr int kPoolSize = 4096;
constexpr float kSpawnRadius = 900.0f; // volume autour de la caméra (~14 tuiles)
constexpr float kSpawnHeight = 500.0f;

// Taux de spawn par seconde à densité 1.0, durée de vie et taille par type.
struct KindDef
{
    float rate, life, size;
    gfx::Color color;
    bool additive;
};
// Indexé par ParticleSystem::Kind (Snow..Splash).
constexpr KindDef kDefs[] = {
    {320.0f, 9.0f, 2.2f, {235, 245, 255, 210}, false}, // Snow
    {60.0f, 9.0f, 4.5f, {200, 120, 40, 235}, false},   // Leaf
    {50.0f, 8.0f, 2.6f, {255, 175, 210, 220}, false},  // Petal
    {25.0f, 7.0f, 3.2f, {180, 255, 120, 255}, true},   // Firefly
    {80.0f, 6.0f, 1.6f, {255, 225, 140, 90}, true},    // Dust
    {90.0f, 2.6f, 2.0f, {255, 120, 30, 220}, true},    // Ember
    {900.0f, 1.4f, 1.0f, {150, 205, 255, 160}, false}, // Rain
    {60.0f, 5.0f, 2.4f, {120, 255, 150, 200}, true},   // Spore
    {0.0f, 0.55f, 1.0f, {170, 215, 255, 150}, false},  // Splash (spawné par Rain)
};
} // namespace

ParticleSystem::ParticleSystem() : _pool(kPoolSize)
{
}

float ParticleSystem::frand()
{
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    return static_cast<float>(_rng & 0xFFFFFF) / 16777215.0f;
}

ParticleSystem::Particle *ParticleSystem::alloc()
{
    for (auto &p : _pool)
        if (p.life <= 0.0f)
            return &p;
    return nullptr;
}

int ParticleSystem::aliveCount() const
{
    int n = 0;
    for (const auto &p : _pool)
        if (p.life > 0.0f)
            ++n;
    return n;
}

void ParticleSystem::spawnKind(std::uint8_t kind, float rate, float dt, gfx::Vec3 camPos, float groundY,
                               bool hasGround, const env::ParticleProfile &prof)
{
    _spawnAcc[kind] += rate * dt;
    while (_spawnAcc[kind] >= 1.0f)
    {
        _spawnAcc[kind] -= 1.0f;
        Particle *p = alloc();
        if (!p)
            return;
        const KindDef &d = kDefs[kind];
        const float ang = frand() * 6.2831853f;
        const float rad = std::sqrt(frand()) * kSpawnRadius;
        p->kind = kind;
        p->maxLife = d.life * (0.7f + 0.6f * frand());
        p->life = p->maxLife;
        p->size = d.size * (0.7f + 0.6f * frand());
        p->rot = frand() * 360.0f;
        p->rotSpeed = (frand() - 0.5f) * 240.0f;
        p->phase = frand() * 6.2831853f;
        p->pos = {camPos.x + std::cos(ang) * rad, 0.0f, camPos.z + std::sin(ang) * rad};
        p->vel = prof.wind;

        switch (kind)
        {
        case Snow:
            p->pos.y = camPos.y + (frand() - 0.2f) * kSpawnHeight;
            p->vel = gfx::add(p->vel, {0, -26.0f - frand() * 14.0f, 0});
            break;
        case Leaf:
            p->pos.y = camPos.y + (frand() - 0.2f) * kSpawnHeight;
            p->vel = gfx::add(p->vel, {0, -20.0f - frand() * 12.0f, 0});
            break;
        case Petal:
            p->pos.y = camPos.y + (frand() - 0.2f) * kSpawnHeight;
            p->vel = gfx::add(p->vel, {0, -12.0f - frand() * 8.0f, 0});
            break;
        case Firefly:
        case Dust:
        case Spore:
            // Suspendues près du sol / de la caméra, flottent sur place.
            p->pos.y = (hasGround ? groundY : camPos.y - 60.0f) + 6.0f + frand() * 90.0f;
            p->vel = gfx::scale(p->vel, 0.15f);
            break;
        case Ember:
            p->pos.y = (hasGround ? groundY : camPos.y - 60.0f) + 2.0f + frand() * 30.0f;
            p->vel = gfx::add(gfx::scale(p->vel, 0.3f), {0, 30.0f + frand() * 25.0f, 0});
            break;
        case Rain:
            p->pos.y = camPos.y + 80.0f + frand() * kSpawnHeight;
            p->vel = gfx::add(p->vel, {0, -420.0f - frand() * 120.0f, 0});
            break;
        default:
            break;
        }
    }
}

void ParticleSystem::update(float dt, const env::ParticleProfile &prof, gfx::Vec3 camPos, float groundY,
                            bool hasGround, float time)
{
    spawnKind(Snow, kDefs[Snow].rate * prof.snow, dt, camPos, groundY, hasGround, prof);
    spawnKind(Leaf, kDefs[Leaf].rate * prof.leaves, dt, camPos, groundY, hasGround, prof);
    spawnKind(Petal, kDefs[Petal].rate * prof.petals, dt, camPos, groundY, hasGround, prof);
    spawnKind(Firefly, kDefs[Firefly].rate * prof.fireflies, dt, camPos, groundY, hasGround, prof);
    spawnKind(Dust, kDefs[Dust].rate * prof.dust, dt, camPos, groundY, hasGround, prof);
    spawnKind(Ember, kDefs[Ember].rate * prof.embers, dt, camPos, groundY, hasGround, prof);
    spawnKind(Rain, kDefs[Rain].rate * prof.rain, dt, camPos, groundY, hasGround, prof);
    spawnKind(Spore, kDefs[Spore].rate * prof.spores, dt, camPos, groundY, hasGround, prof);

    for (auto &p : _pool)
    {
        if (p.life <= 0.0f)
            continue;
        p.life -= dt;
        p.rot += p.rotSpeed * dt;

        switch (p.kind)
        {
        case Snow:
        case Petal:
            // Dérive sinusoïdale latérale : flocons/pétales qui zigzaguent.
            p.pos.x += std::sin(time * 1.6f + p.phase) * 14.0f * dt;
            p.pos.z += std::cos(time * 1.3f + p.phase) * 10.0f * dt;
            break;
        case Leaf:
            // Tourbillon plus large + chute qui « plane » par instants.
            p.pos.x += std::sin(time * 2.1f + p.phase) * 30.0f * dt;
            p.pos.z += std::cos(time * 1.7f + p.phase) * 22.0f * dt;
            break;
        case Firefly:
            p.pos.x += std::sin(time * 0.9f + p.phase) * 20.0f * dt;
            p.pos.y += std::sin(time * 1.4f + p.phase * 2.0f) * 12.0f * dt;
            p.pos.z += std::cos(time * 1.1f + p.phase) * 20.0f * dt;
            break;
        case Dust:
        case Spore:
            p.pos.y += std::sin(time * 0.7f + p.phase) * 5.0f * dt;
            break;
        default:
            break;
        }
        p.pos = gfx::add(p.pos, gfx::scale(p.vel, dt));

        // Contact sol : la neige se dépose (fondu), la pluie éclabousse.
        if (hasGround && p.pos.y <= groundY)
        {
            if (p.kind == Rain)
            {
                p.life = 0.0f;
                if (Particle *s = alloc())
                {
                    *s = {};
                    s->kind = Splash;
                    s->pos = {p.pos.x, groundY + 0.5f, p.pos.z};
                    s->maxLife = kDefs[Splash].life;
                    s->life = s->maxLife;
                    s->size = 3.0f;
                }
            }
            else if (p.kind == Snow || p.kind == Leaf || p.kind == Petal)
            {
                p.pos.y = groundY + 0.4f;
                p.vel = {0, 0, 0};
                p.life = std::min(p.life, 1.6f); // repose puis fond
            }
            else if (p.kind != Splash)
            {
                p.pos.y = groundY + 1.0f;
            }
        }
    }
}

void ParticleSystem::draw(RaylibEngine &engine, const gfx::Camera &cam, gfx::TextureHandle softDot) const
{
    // Base billboard face caméra, partagée par toutes les particules.
    const gfx::Vec3 fwd = gfx::normalize(gfx::sub(cam.target, cam.position));
    gfx::Vec3 right = gfx::normalize({-fwd.z, 0.0f, fwd.x});
    const gfx::Vec3 up = {fwd.y * right.z - fwd.z * right.y, fwd.z * right.x - fwd.x * right.z,
                          fwd.x * right.y - fwd.y * right.x};

    auto quad = [&](const Particle &p, gfx::Color c, float stretch) {
        // Rotation du billboard dans son plan (feuilles qui tournoient).
        const float cr = std::cos(p.rot * 0.01745329f), sr = std::sin(p.rot * 0.01745329f);
        const gfx::Vec3 r = gfx::add(gfx::scale(right, cr * p.size), gfx::scale(up, sr * p.size));
        const gfx::Vec3 u = gfx::add(gfx::scale(right, -sr * p.size * stretch), gfx::scale(up, cr * p.size * stretch));
        const gfx::Vec3 a = gfx::add(gfx::add(p.pos, gfx::scale(r, -1)), u);
        const gfx::Vec3 b = gfx::sub(gfx::add(p.pos, gfx::scale(r, -1)), u);
        const gfx::Vec3 cq = gfx::sub(gfx::add(p.pos, r), u);
        const gfx::Vec3 d = gfx::add(gfx::add(p.pos, r), u);
        engine.drawTexturedQuad3D(softDot, a, b, cq, d, gfx::scale(fwd, -1.0f), c);
    };

    // Deux passes : alpha classique puis additif (émissives, bloom-friendly).
    for (int pass = 0; pass < 2; ++pass)
    {
        engine.setAdditiveBlend(pass == 1);
        for (const auto &p : _pool)
        {
            if (p.life <= 0.0f)
                continue;
            const KindDef &def = kDefs[p.kind];
            if ((pass == 1) != def.additive)
                continue;

            const float fade = std::min(1.0f, p.life / (p.maxLife * 0.25f));
            gfx::Color c = def.color;
            c.a = static_cast<std::uint8_t>(c.a * fade);

            if (p.kind == Rain)
            {
                // Trait étiré le long de la vitesse — pas de quad.
                engine.drawLine3D(p.pos, gfx::add(p.pos, gfx::scale(p.vel, 0.035f)), c);
                continue;
            }
            if (p.kind == Splash)
            {
                // Anneau qui s'étend au sol.
                const float t = 1.0f - p.life / p.maxLife;
                c.a = static_cast<std::uint8_t>(150.0f * (1.0f - t));
                engine.drawCircle3D(p.pos, p.size + t * 9.0f, c);
                continue;
            }
            if (p.kind == Firefly)
            {
                // Clignotement : au-dessus du seuil de bloom quand allumée.
                const float blink = 0.5f + 0.5f * std::sin(p.life * 4.0f + p.phase * 3.0f);
                c.a = static_cast<std::uint8_t>(c.a * (0.25f + 0.75f * blink));
            }
            quad(p, c, p.kind == Leaf ? 0.6f : 1.0f);
        }
    }
    engine.setAdditiveBlend(false);
}
```

- [ ] **Step 4: Interface — brancher, supprimer l'overlay 2D**

`gui/interface.hpp` :
- Ajouter `#include "particles.hpp"`.
- Membres :

```cpp
    ParticleSystem _particles;
    gfx::TextureHandle _particleDot{gfx::NoHandle}; // disque doux généré
```

- SUPPRIMER la déclaration `void drawWeatherOverlay();`.

`gui/interface.cpp` :
- Constructeur (après le chargement des textures soleil/lune) : `_particleDot = _engine.createRadialTexture(64);`
- `update()` (après `_env.update(dt)`) :

```cpp
    if (_weatherVisible)
    {
        env::ParticleProfile prof = _env.snap().particles;
        _particles.update(dt, prof, _camera.position, kTileTopY, !_torusView, _elapsed);
    }
```

- `render()` : dans la section 3D, juste avant `_engine.endMode3D();` (après `drawSelectionHighlight()`) :

```cpp
    if (_weatherVisible)
        _particles.draw(_engine, _camera, _particleDot);
```

- SUPPRIMER : la fonction `Interface::drawWeatherOverlay()` entière (lignes ~2246-2369) et son appel `if (_weatherVisible) drawWeatherOverlay();` (ligne ~1626-1627).
- Overlay perf (`drawPerfOverlay`) : ajouter une ligne `particles: N` via `_particles.aliveCount()` (suivre le format des lignes existantes).

`gui/CMakeLists.txt` : ajouter `particles.cpp` aux sources de `zappy_gui`.

- [ ] **Step 5: Build + vérification visuelle**

Run: `make zappy_gui 2>&1 | tail -3` — Expected: OK.
Vérifier (avec F5/F6/F7) :
- Hiver : neige 3D qui dérive et se POSE sur le sol (reste ~1,5 s puis fond). Les flocons proches sont devant les robots, les lointains derrière (vraie 3D, plus un overlay).
- Automne : feuilles qui tourbillonnent en rotation. Printemps de nuit : lucioles clignotantes qui BLOOMENT. Été : poussière dorée ; heat : braises montantes.
- Pluie : traits 3D + anneaux d'éclaboussure au sol (mode grille). Orage : pluie dense + vent fort visible.
- Mode tore (T) : particules toujours présentes autour de la caméra, pas de splash (attendu).
- V coupe toutes les particules. F3 : compteur `particles` cohérent (< 4096).

- [ ] **Step 6: Commit**

```bash
git add gui/particles.hpp gui/particles.cpp gui/raylibWrapper.hpp gui/raylibWrapper.cpp \
        gui/interface.hpp gui/interface.cpp gui/CMakeLists.txt
git commit -m "feat(gui): particules 3D monde (neige, feuilles, lucioles, pluie...) — remplace l'overlay 2D"
```

---

### Task 6: Shadow mapping (ombres portées PCF, soleil le jour / lune la nuit)

**Files:**
- Modify: `gui/assets/shaders/lighting.fs` + `gui/assets/shaders/floor.fs` (échantillonnage shadow map PCF)
- Modify: `gui/raylibWrapper.hpp/.cpp` (`enableShadows`, `beginShadowPass`, `endShadowPass`, `shadowsReady`)
- Modify: `gui/interface.hpp/.cpp` (extraction `drawWorld3D(bool depthPass)`, passe d'ombre dans `render()`)

**Interfaces:**
- Consumes: `es.activeLightDir` (Task 1/2), shaders lighting/floor (Task 2), le corps 3D de `render()`.
- Produces: `bool RaylibEngine::enableShadows(int resolution)` ; `bool RaylibEngine::shadowsReady() const` ; `void RaylibEngine::beginShadowPass(gfx::Vec3 lightDir, gfx::Vec3 sceneCentre, float sceneRadius)` ; `void RaylibEngine::endShadowPass()` ; `void Interface::drawWorld3D(bool depthPass)` (floor + tuiles + joueurs + ressources + météorites ; PAS le ciel/particules/highlights).

Implémentation calquée sur l'exemple officiel raylib `shaders_shadowmap.c` : FBO depth-only + `BeginTextureMode` + `BeginMode3D(lightCam)` ortho (l'aspect vaut 1 car BeginTextureMode fixe le framebuffer courant à la taille de la texture), matrices récupérées via `rlGetMatrixModelview()`/`rlGetMatrixProjection()`, shadow map bindée sur le slot 10 après la passe.

- [ ] **Step 1: Shaders — PCF 3×3**

Dans `gui/assets/shaders/lighting.fs` ET `gui/assets/shaders/floor.fs`, ajouter les uniforms :

```glsl
uniform sampler2D shadowMap;
uniform mat4 lightVP;
uniform int shadowsOn;   // 0 tant qu'aucune passe n'a tourné
uniform int shadowMapRes;
```

et la fonction (avant `main`) :

```glsl
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
```

Usage — `lighting.fs`, remplacer la ligne `vec3 lit = ...` :

```glsl
    float shadow = shadowFactor(fragPosition, n, l);
    vec3 lit = texel.rgb * (ambientColor + sunColor * diff * shadow)
             + sunColor * (spec * shadow + 0.18 * rim * diff)
             + ambientColor * 0.35 * rim;
```

`floor.fs`, remplacer `vec3 lit = seasonal * (ambientColor + sunColor * diff);` :

```glsl
    float shadow = shadowFactor(fragPosition, n, -normalize(sunDir));
    vec3 lit = seasonal * (ambientColor + sunColor * diff * shadow);
```

- [ ] **Step 2: RaylibEngine — passes d'ombre**

`gui/raylibWrapper.hpp` (section Scene shading) :

```cpp
    // Shadow mapping : passe de profondeur ortho depuis la lumière active.
    // Usage par frame : beginShadowPass -> (draws du monde) -> endShadowPass,
    // puis rendu normal ; endShadowPass pousse lightVP + la depth map (slot 10)
    // sur les shaders lighting / instancing / floor.
    bool enableShadows(int resolution);
    bool shadowsReady() const;
    void beginShadowPass(gfx::Vec3 lightDir, gfx::Vec3 sceneCentre, float sceneRadius);
    void endShadowPass();
```

`gui/raylibWrapper.cpp` :

1. Comme pour `SceneLocs` (Task 2), déclarer le type au niveau fichier, dans le namespace anonyme au-dessus de `struct RaylibEngine::Impl` :

```cpp
// Locations des uniforms d'ombre par shader (lighting / instancing / floor).
struct ShadowLocs
{
    int map{-1}, vp{-1}, on{-1}, res{-1};
};
```

Puis dans `Impl` :

```cpp
    // Shadow map (FBO depth-only) + locations par shader.
    RenderTexture2D shadowRT{};
    bool shadowsLoaded{false};
    int shadowRes{0};
    Matrix lightVP{};
    ShadowLocs lightShadow{}, instShadow{}, floorShadow{};
```

2. Helpers (namespace anonyme) :

```cpp
namespace
{
// FBO sans couleur : seulement une texture de profondeur échantillonnable.
RenderTexture2D loadShadowmapRT(int width, int height)
{
    RenderTexture2D target{};
    target.id = rlLoadFramebuffer();
    if (target.id == 0)
        return target;
    target.texture.width = width;
    target.texture.height = height;
    rlEnableFramebuffer(target.id);
    target.depth.id = rlLoadTextureDepth(width, height, false); // texture, pas renderbuffer
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19;
    target.depth.mipmaps = 1;
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
    if (!rlFramebufferComplete(target.id))
    {
        rlUnloadFramebuffer(target.id);
        target.id = 0;
    }
    rlDisableFramebuffer();
    return target;
}

ShadowLocs cacheShadowLocs(Shader s)
{
    ShadowLocs l;
    l.map = GetShaderLocation(s, "shadowMap");
    l.vp = GetShaderLocation(s, "lightVP");
    l.on = GetShaderLocation(s, "shadowsOn");
    l.res = GetShaderLocation(s, "shadowMapRes");
    return l;
}
} // namespace
```

3. Méthodes :

```cpp
bool RaylibEngine::enableShadows(int resolution)
{
    _impl->shadowRT = loadShadowmapRT(resolution, resolution);
    if (_impl->shadowRT.id == 0)
    {
        TraceLog(LOG_WARNING, "enableShadows: depth FBO failed — no shadows");
        return false;
    }
    _impl->shadowRes = resolution;
    _impl->shadowsLoaded = true;
    if (_impl->lightLoaded)
        _impl->lightShadow = cacheShadowLocs(_impl->lightShader);
    if (_impl->instLoaded)
        _impl->instShadow = cacheShadowLocs(_impl->instShader);
    if (_impl->floorLoaded)
        _impl->floorShadow = cacheShadowLocs(_impl->floorShader);
    TraceLog(LOG_INFO, "enableShadows: %dx%d depth map ready", resolution, resolution);
    return true;
}

bool RaylibEngine::shadowsReady() const
{
    return _impl->shadowsLoaded;
}

void RaylibEngine::beginShadowPass(gfx::Vec3 lightDir, gfx::Vec3 sceneCentre, float sceneRadius)
{
    if (!_impl->shadowsLoaded)
        return;
    Camera3D lightCam{};
    const gfx::Vec3 d = gfx::normalize(lightDir);
    lightCam.position = toRl(gfx::sub(sceneCentre, gfx::scale(d, sceneRadius * 1.8f)));
    lightCam.target = toRl(sceneCentre);
    lightCam.up = std::fabs(d.y) > 0.95f ? Vector3{1, 0, 0} : Vector3{0, 1, 0};
    lightCam.fovy = sceneRadius * 2.0f; // largeur du volume ortho
    lightCam.projection = CAMERA_ORTHOGRAPHIC;

    BeginTextureMode(_impl->shadowRT); // fixe viewport + framebuffer taille shadowRes
    ClearBackground(WHITE);            // depth clear (la couleur n'existe pas)
    BeginMode3D(lightCam);
    _impl->lightVP = MatrixMultiply(rlGetMatrixModelview(), rlGetMatrixProjection());
}

void RaylibEngine::endShadowPass()
{
    if (!_impl->shadowsLoaded)
        return;
    EndMode3D();
    EndTextureMode();

    // Publier la depth map (slot 10) + lightVP sur les trois shaders scène.
    const int slot = 10;
    const int on = 1;
    auto push = [&](Shader s, const ShadowLocs &l) {
        if (l.vp >= 0)
            SetShaderValueMatrix(s, l.vp, _impl->lightVP);
        if (l.on >= 0)
            SetShaderValue(s, l.on, &on, SHADER_UNIFORM_INT);
        if (l.res >= 0)
            SetShaderValue(s, l.res, &_impl->shadowRes, SHADER_UNIFORM_INT);
        if (l.map >= 0)
        {
            rlEnableShader(s.id);
            rlActiveTextureSlot(slot);
            rlEnableTexture(_impl->shadowRT.depth.id);
            rlSetUniform(l.map, &slot, SHADER_UNIFORM_INT, 1);
            rlActiveTextureSlot(0);
        }
    };
    if (_impl->lightLoaded)
        push(_impl->lightShader, _impl->lightShadow);
    if (_impl->instLoaded)
        push(_impl->instShader, _impl->instShadow);
    if (_impl->floorLoaded)
        push(_impl->floorShader, _impl->floorShadow);
}
```

Destructeur : `if (_impl->shadowsLoaded) UnloadRenderTexture(_impl->shadowRT);`

- [ ] **Step 3: Interface — extraire drawWorld3D + double passe**

`gui/interface.hpp` : déclarer `void drawWorld3D(bool depthPass);` (près de `render()`).

`gui/interface.cpp` :

1. Constructeur : après `enablePostFx(...)` : `_engine.enableShadows(4096);`

2. Extraire de `render()` le bloc « monde » dans `drawWorld3D(bool depthPass)`. Contenu déplacé : le sol (torus ou checker), la double boucle tuiles (edges/joueurs/ressources/œufs/LOD), le flush des `playerDraws` + instanced, `drawMeteorites()`, le modèle d'incantation. Règles :
   - `depthPass == true` : NE PAS dessiner : skybox, celestials, `drawTileEdges`, labels (ne pas push), `drawIncantationRings`, `drawHoverHighlight`, `drawSelectionHighlight`, particules. NE PAS incrémenter `_stats` (garder les compteurs uniquement sur la passe principale : `if (!depthPass) ++_stats...`).
   - Les vecteurs `labels`, `playerDraws`, `itemXf` deviennent des locaux de `drawWorld3D` ; `labels` doit être remonté à `render()` pour la projection écran → en faire un membre temporaire `std::vector<CountLabel> _frameLabels` (vidé en début de `drawWorld3D(false)`, ignoré en depth pass) — `CountLabel` est la struct locale existante `{worldPos, count, color}` à déplacer dans `interface.hpp` (section privée).
   - Le culling frustum existant reste actif sur la passe principale ; sur la depth pass, cull avec le même test mais depuis la lumière : le plus simple et sûr est de NE PAS culler en depth pass (`tileVisible` → `depthPass || ...` en tête). Les ombres d'objets hors champ restent alors correctes.

3. Nouveau `render()` (structure) :

```cpp
void Interface::render()
{
    _stats = {};
    const env::Snapshot &es = _env.snap();
    // ... (bloc setSceneLighting / setGroundSeason / setPostFxParams des tâches 2 et 4, inchangé)

    // Passe 1 : profondeur depuis la lumière active (soleil ou lune).
    if (_engine.shadowsReady())
    {
        gfx::Vec3 centre;
        float radius;
        if (_torusView)
        {
            const TorusGeom g = torusGeom();
            centre = g.c;
            radius = g.R + g.r + TILE_SIZE;
        }
        else
        {
            const float w = _map.getWidth() * TILE_SIZE, h = _map.getHeight() * TILE_SIZE;
            centre = {w * 0.5f, 0.0f, h * 0.5f};
            radius = 0.5f * std::sqrt(w * w + h * h) + TILE_SIZE * 2.0f;
        }
        _engine.beginShadowPass(es.activeLightDir, centre, radius);
        drawWorld3D(true);
        _engine.endShadowPass();
    }

    // Passe 2 : scène complète.
    _engine.beginMode3D(_camera);
    // ... (setSkyParams + drawSkybox + drawCelestial, Task 3, inchangé)
    drawWorld3D(false);
    drawMeteorites();      // si non déplacés dans drawWorld3D — voir note
    drawIncantationRings();
    drawHoverHighlight();
    drawSelectionHighlight();
    if (_weatherVisible)
        _particles.draw(_engine, _camera, _particleDot);
    _engine.endMode3D();

    // ... (2D : labels via _frameLabels, HUD, feed, panels — inchangé)
}
```

Note : `drawMeteorites()` DOIT être dans `drawWorld3D` (les météorites projettent des ombres) — le retirer de la liste post-drawWorld3D ci-dessus et le garder dans l'extraction, gardé par rien (il dessine aussi la traînée de feu émissive : en depth pass la traînée est un quad glow sans intérêt — l'entourer de `if (!depthPass)` à l'intérieur en gardant le rocher).

- [ ] **Step 4: Build + vérification visuelle**

Run: `make zappy_gui 2>&1 | tail -3` — Expected: OK, log `enableShadows: 4096x4096 depth map ready`.
Vérifier :
- Robots, ressources et météorites projettent des ombres au sol, orientées à l'opposé du soleil visible dans le ciel.
- F7 accéléré : les ombres TOURNENT avec le soleil (longues à l'aube/crépuscule, courtes à midi) ; la nuit, ombres douces au clair de lune.
- Pas d'acné d'ombre (moirage) ni de peter-panning grossier ; ajuster le bias si besoin (noter la valeur finale).
- Mode tore : ombres présentes sur le donut. FPS : vérifier F3 — la passe depth ne doit pas faire chuter sous ~60 sur la machine cible.

- [ ] **Step 5: Commit**

```bash
git add gui/assets/shaders/lighting.fs gui/assets/shaders/floor.fs \
        gui/raylibWrapper.hpp gui/raylibWrapper.cpp gui/interface.hpp gui/interface.cpp
git commit -m "feat(gui): shadow mapping PCF 4096 — ombres solaires/lunaires dynamiques"
```

---

### Task 7: Sol saisonnier en mode tore, polish, aide & doc

**Files:**
- Modify: `gui/interface.cpp` (`drawTorusFloor` piloté par env, aide, feed)
- Modify: `gui/interface.hpp` (si signatures touchées)

**Interfaces:**
- Consumes: `es.groundOverlay`, `es.groundMix`, `es.activeLightDir`, `es.sunColor`, `es.ambient` (Task 1), tout le pipeline des tâches 2-6.
- Produces: rendu final complet ; aucune nouvelle interface.

- [ ] **Step 1: drawTorusFloor saisonnier**

Dans `Interface::drawTorusFloor()` (interface.cpp ~739) : remplacer la lumière analytique et les couleurs codées en dur pour suivre l'environnement (le sol torique passe par des quads immédiats, pas par le floor shader — la teinte se calcule CPU par sous-quad) :

```cpp
void Interface::drawTorusFloor()
{
    const int mapW = _map.getWidth(), mapH = _map.getHeight();
    const int subU = torusSubdiv(mapW);
    const int subV = torusSubdiv(mapH);
    const env::Snapshot &es = _env.snap();
    // Lambert analytique sur la lumière active (les quads immédiats ne passent
    // pas par le lighting shader).
    const gfx::Vec3 lightDir = gfx::scale(gfx::normalize(es.activeLightDir), -1.0f);
    const float lightLum = std::min(1.0f, (es.sunColor.x + es.sunColor.y + es.sunColor.z) / 3.0f + 0.15f);
    const float ambLum = (es.ambient.x + es.ambient.y + es.ambient.z) / 3.0f;
    const gfx::Vec3 overlay = es.groundOverlay;
    const float mixAmt = _weatherVisible ? es.groundMix : 0.0f;
    const gfx::Color dark{55, 78, 54, 255}, light{122, 190, 82, 255};

    for (int y = 0; y < mapH; ++y)
    {
        for (int x = 0; x < mapW; ++x)
        {
            const bool blackGrass = ((x + y) & 1) != 0;
            const gfx::Color base = blackGrass ? dark : light;
            const gfx::TextureHandle tex = blackGrass ? _blackGrassTileTexture : _grassTileTexture;
            for (int j = 0; j < subV; ++j)
            {
                for (int i = 0; i < subU; ++i)
                {
                    const float u0 = (x + static_cast<float>(i) / subU) * TILE_SIZE;
                    const float u1 = (x + static_cast<float>(i + 1) / subU) * TILE_SIZE;
                    const float v0 = (y + static_cast<float>(j) / subV) * TILE_SIZE;
                    const float v1 = (y + static_cast<float>(j + 1) / subV) * TILE_SIZE;

                    const Surface mid = surfaceAt((u0 + u1) * 0.5f, (v0 + v1) * 0.5f, 0.0f);
                    const float lum =
                        std::min(1.0f, ambLum + lightLum * std::max(0.0f, gfx::dot(mid.up, lightDir)));

                    // Teinte saison : texture -> overlay (neige, or, roux...).
                    auto shade = [&](float channelBase, float overlayC) {
                        const float c = channelBase * (1.0f - mixAmt) + overlayC * 255.0f * mixAmt;
                        return static_cast<std::uint8_t>(std::min(255.0f, c * lum));
                    };
                    const gfx::Color texTint{shade(255.0f, overlay.x), shade(255.0f, overlay.y),
                                             shade(255.0f, overlay.z), 255};
                    const gfx::Color flatTint{shade(base.r, overlay.x), shade(base.g, overlay.y),
                                              shade(base.b, overlay.z), 255};

                    const gfx::Vec3 a = surfaceAt(u0, v0, 0.0f).pos;
                    const gfx::Vec3 b = surfaceAt(u1, v0, 0.0f).pos;
                    const gfx::Vec3 cpos = surfaceAt(u1, v1, 0.0f).pos;
                    const gfx::Vec3 d = surfaceAt(u0, v1, 0.0f).pos;
                    if (tex != gfx::NoHandle)
                        _engine.drawTexturedQuad3D(tex, a, b, cpos, d, mid.up, texTint);
                    else
                        _engine.drawQuad3D(a, b, cpos, d, flatTint);
                }
            }
        }
    }
}
```

- [ ] **Step 2: Aide (H) + feed**

`drawHelpOverlay()` (~ligne 2605) : remplacer `"V               toggle weather visuals"` par :

```cpp
            "V               toggle seasonal FX (particles, fog, grading)",
            "F5 / F6         force season / weather (debug)",
            "F7              time-lapse day/night x40",
```

Vérifier que le feed « Weather » (ligne ~2047-2053) affiche toujours correctement les transitions — inchangé, juste re-tester.

- [ ] **Step 3: Vérification finale complète (matrice de recette)**

Run: `make zappy_gui && ctest --test-dir build -R gui_environment --output-on-failure` — Expected: build OK + tests PASS.
Lancer serveur + GUI, dérouler la matrice (F5 × F6 × F7) et confirmer :
1. 4 saisons × {clear, rain, storm, fog, heat, fertile} : particules, ciel, grading, sol cohérents, transitions fondues sans pop.
2. Cycle complet jour → nuit en accéléré, grille ET tore : soleil/lune se lèvent/couchent, ombres suivent, aurores l'hiver la nuit, lucioles au printemps la nuit.
3. Lisibilité : HUD, feed, bulles de broadcast, panneau tuile, timeline, end screen restent lisibles dans toutes les combinaisons (surtout storm de nuit — vérifier le contraste).
4. Interactions intactes : sélection/hover de tuile, follow (F), scrub timeline (PageUp/Down), incantations, météorites (+ leurs ombres), musique (M).
5. Perf (F3) : ~60 FPS en 1920×950 sur la machine de dev, carte 12×12 avec joueurs.
6. Fallbacks : lancer une fois avec `gui/assets/shaders` renommé → le GUI démarre en rendu dégradé sans crash (warnings dans les logs), puis restaurer.

- [ ] **Step 4: Commit final**

```bash
git add gui/interface.cpp gui/interface.hpp
git commit -m "feat(gui): sol torique saisonnier + aide debug — refonte visuelle complète"
```

---

## Self-Review Notes

- Couverture spec : EnvironmentState+interpolation (T1), cycle jour/nuit Luan/Palasse (T1+T3), ciel procédural/étoiles/nébuleuses/aurores/éclairs (T3), lumière dynamique uniforms + rim + fog + flash 3D (T2), shadow mapping PCF soleil/lune (T6), particules 3D 8 types + splash + suppression overlay 2D (T5), post ACES/god rays/grading/vignette/heat + RT 16F (T4), sol saisonnier flat (T2, floor.fs) et tore (T7), touche debug (T2), fallbacks partout, touche V re-routée (T2/T4/T5).
- Les valeurs numériques (couleurs, densités, biais d'ombre, forces) sont des points de départ calibrés à l'estime : l'implémenteur DOIT les ajuster à l'œil pendant les vérifications visuelles et commiter les valeurs finales.
- Deux pièges connus signalés dans les steps : orientation Y de `sunScreen` (god rays) et nom exact de `GenImageGradientRadial` selon la version raylib.

