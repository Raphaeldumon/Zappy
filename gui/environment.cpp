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
