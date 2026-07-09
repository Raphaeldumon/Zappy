#pragma once

// ---------------------------------------------------------------------------
// GroundCover — accumulation persistante au sol (neige, feuilles, eau) +
// traces de passage des robots.
//
// Un buffer RGBA8 couvre toute la carte (N texels par tuile) :
//   R = épaisseur de neige      (s'accumule quand il neige, fond sinon)
//   G = litière de feuilles     (s'accumule en automne, se décompose)
//   B = eau / sol détrempé      (pluie -> flaques, sèche ensuite)
//   A = trace pressée           (robots ; se ré-efface, la neige la recouvre)
//
// La sim tourne à tick fixe sur CPU (buffer minuscule), les robots tamponnent
// leur passage chaque frame, et le buffer est uploadé en texture que floor.fs
// échantillonne en coordonnées monde. sample() donne la même info au sol
// torique (teinte CPU).
// ---------------------------------------------------------------------------

#include "environment.hpp"
#include <cstdint>
#include <vector>

class GroundCover
{
  public:
    // mapW/mapH en tuiles, tileSize en unités monde.
    GroundCover(int mapW, int mapH, float tileSize);

    // Avance l'accumulation/fonte/séchage (tick fixe interne, dt réel accepté).
    void update(float dt, const env::ParticleProfile &prof, float heat);

    // Passage d'un robot en (wx, wz) monde : compresse neige/feuilles et
    // marque la trace. À appeler chaque frame par robot en mouvement.
    void stampTrail(float wx, float wz, float radius);

    // Valeurs 0..1 interpolées au point monde (teinte CPU du tore).
    struct Sample
    {
        float snow, leaf, wet, trail;
    };
    Sample sample(float wx, float wz) const;

    const std::uint8_t *pixels() const { return _px.data(); }
    int width() const { return _w; }
    int height() const { return _h; }
    // true si le buffer a changé depuis le dernier appel (upload nécessaire).
    bool consumeDirty();

  private:
    int _w{0}, _h{0};        // texels
    float _texelsPerUnit{0}; // texels par unité monde
    float _tickAcc{0.0f};
    bool _dirty{true};
    std::vector<std::uint8_t> _px;    // RGBA8, ligne par ligne
    std::vector<std::uint8_t> _speed; // pondération spatiale 0..255 (accumulation inégale)

    void tick(float dt, const env::ParticleProfile &prof, float heat);
};
