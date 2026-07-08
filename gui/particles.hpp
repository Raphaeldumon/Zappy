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
