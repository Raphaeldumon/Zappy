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
    gfx::Vec3 skyDayTint{0.18f, 0.38f, 0.75f}; // zénith du ciel diurne (par saison)
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
    gfx::Vec3 skyDayTint{0.18f, 0.38f, 0.75f};
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
    // Paramètres de trajectoire d'un astre, gelés pendant son vol : rafraîchis
    // seulement sous l'horizon (saut invisible) pour que le déplacement reste
    // continu et linéaire du lever au coucher. fraction <= 0 = pas encore init.
    struct Flight
    {
        float fraction{0};
        float maxElev{0};
    };

    std::string _season{"spring"}, _weather{"clear"};
    Profile _cur{profileFor("spring", "clear")};
    Profile _tgt{_cur};
    Flight _sunFlight{};
    Flight _moonFlight{};
    Snapshot _snap{};
    float _timeOfDay{0.35f}; // démarre en matinée
    float _timeScale{1.0f};
    float _flash{0};
    float _nextFlashIn{4.0f};
};

} // namespace env
