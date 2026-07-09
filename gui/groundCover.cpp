#include "groundCover.hpp"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kTickSeconds = 0.125f; // 8 Hz : bien assez pour de la météo

// Taux par seconde (valeurs 0..1 par canal).
constexpr float kSnowFall = 0.030f;  // neige pleine en ~55 s à densité 0.6
constexpr float kSnowMelt = 0.009f;  // fonte hors neige (~2 min)
constexpr float kLeafFall = 0.034f;  // tapis complet en ~40 s à densité 0.7
constexpr float kLeafRot = 0.005f;   // décomposition hors automne
constexpr float kRainSoak = 0.110f;  // sol détrempé en ~13 s sous pluie forte
constexpr float kDryRate = 0.011f;   // séchage (~90 s)
constexpr float kTrailFade = 0.014f; // les traces s'estompent en ~70 s

std::uint32_t hash2(std::uint32_t x, std::uint32_t y)
{
    std::uint32_t h = x * 0x8da6b343u + y * 0xd8163841u;
    h ^= h >> 13;
    h *= 0x9e3779b1u;
    h ^= h >> 16;
    return h;
}
float hash01(std::uint32_t x, std::uint32_t y)
{
    return static_cast<float>(hash2(x, y) & 0xFFFF) / 65535.0f;
}
} // namespace

GroundCover::GroundCover(int mapW, int mapH, float tileSize)
{
    // Assez de texels pour des traces lisibles, plafonné pour les grandes maps.
    const int side = std::max(mapW, mapH);
    const int tpt = std::clamp(512 / std::max(side, 1), 4, 12);
    _w = std::max(mapW * tpt, 4);
    _h = std::max(mapH * tpt, 4);
    _texelsPerUnit = static_cast<float>(tpt) / tileSize;
    _px.assign(static_cast<std::size_t>(_w) * _h * 4, 0);

    // Pondération spatiale : bruit de valeur bilinéaire (maille ~1 tuile) +
    // jitter fin, pour que neige/feuilles s'installent par plaques naturelles.
    _speed.resize(static_cast<std::size_t>(_w) * _h);
    const float cell = static_cast<float>(tpt);
    for (int y = 0; y < _h; ++y)
    {
        for (int x = 0; x < _w; ++x)
        {
            const float fx = x / cell, fy = y / cell;
            const int ix = static_cast<int>(fx), iy = static_cast<int>(fy);
            const float tx = fx - ix, ty = fy - iy;
            const float a = hash01(ix, iy), b = hash01(ix + 1, iy);
            const float c = hash01(ix, iy + 1), d = hash01(ix + 1, iy + 1);
            const float coarse = (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty;
            const float fine = hash01(x * 7 + 3, y * 13 + 5);
            const float w01 = 0.55f + 0.75f * (0.75f * coarse + 0.25f * fine); // 0.55..1.3
            _speed[static_cast<std::size_t>(y) * _w + x] = static_cast<std::uint8_t>(w01 * 160.0f);
        }
    }
}

void GroundCover::update(float dt, const env::ParticleProfile &prof, float heat)
{
    _tickAcc += dt;
    // Rattrapage borné : si la frame a duré, on ne rejoue que 4 ticks max.
    int steps = 0;
    while (_tickAcc >= kTickSeconds && steps < 4)
    {
        _tickAcc -= kTickSeconds;
        tick(kTickSeconds, prof, heat);
        ++steps;
    }
    if (steps == 4)
        _tickAcc = 0.0f;
}

void GroundCover::tick(float dt, const env::ParticleProfile &prof, float heat)
{
    const bool snowing = prof.snow > 0.05f;
    const bool raining = prof.rain > 0.05f;
    const bool leafing = prof.leaves > 0.05f;

    // Deltas par tick, en unités de canal (0..255).
    const float snowUp = kSnowFall * prof.snow * dt * 255.0f;
    const float snowDn = (kSnowMelt + heat * 0.045f + prof.rain * 0.020f) * dt * 255.0f;
    const float leafUp = kLeafFall * prof.leaves * dt * 255.0f;
    const float leafDn = (kLeafRot + prof.rain * 0.008f) * dt * 255.0f;
    const float wetUp = kRainSoak * prof.rain * dt * 255.0f;
    const float wetDn = (kDryRate + heat * 0.040f) * dt * 255.0f;
    const float trailDn = (kTrailFade + kSnowFall * prof.snow * 1.5f) * dt * 255.0f;

    std::uint8_t *p = _px.data();
    const std::uint8_t *w = _speed.data();
    const std::size_t n = static_cast<std::size_t>(_w) * _h;
    for (std::size_t i = 0; i < n; ++i, p += 4)
    {
        const float k = w[i] / 160.0f; // 0.55..1.3
        float snow = p[0], leaf = p[1], wet = p[2], trail = p[3];

        snow += snowing ? snowUp * k : -snowDn;
        leaf += leafing ? leafUp * k : -leafDn;
        // La fonte mouille le sol ; la pluie aussi.
        if (!snowing && snow > 0.0f)
            wet += snowDn * 0.5f;
        wet += raining ? wetUp * (0.7f + 0.3f * k) : -wetDn;
        trail -= trailDn;

        p[0] = static_cast<std::uint8_t>(std::clamp(snow, 0.0f, 255.0f));
        p[1] = static_cast<std::uint8_t>(std::clamp(leaf, 0.0f, 255.0f));
        p[2] = static_cast<std::uint8_t>(std::clamp(wet, 0.0f, 255.0f));
        p[3] = static_cast<std::uint8_t>(std::clamp(trail, 0.0f, 255.0f));
    }
    _dirty = true;
}

void GroundCover::stampTrail(float wx, float wz, float radius)
{
    const float cx = wx * _texelsPerUnit - 0.5f;
    const float cy = wz * _texelsPerUnit - 0.5f;
    const float r = std::max(radius * _texelsPerUnit, 1.0f);
    const int x0 = static_cast<int>(std::floor(cx - r)), x1 = static_cast<int>(std::ceil(cx + r));
    const int y0 = static_cast<int>(std::floor(cy - r)), y1 = static_cast<int>(std::ceil(cy + r));

    for (int y = y0; y <= y1; ++y)
    {
        for (int x = x0; x <= x1; ++x)
        {
            // Wrap torique : la carte boucle sur les deux axes.
            const int mx = ((x % _w) + _w) % _w;
            const int my = ((y % _h) + _h) % _h;
            const float dx = x - cx, dy = y - cy;
            const float d2 = (dx * dx + dy * dy) / (r * r);
            if (d2 > 1.0f)
                continue;
            const float fall = 1.0f - d2 * d2; // plein au centre, doux au bord
            std::uint8_t *p = &_px[(static_cast<std::size_t>(my) * _w + mx) * 4];
            // Neige compressée / feuilles balayées, proportionnellement.
            p[0] = static_cast<std::uint8_t>(p[0] * (1.0f - 0.55f * fall));
            p[1] = static_cast<std::uint8_t>(p[1] * (1.0f - 0.70f * fall));
            p[3] = std::max(p[3], static_cast<std::uint8_t>(230.0f * fall));
        }
    }
    _dirty = true;
}

GroundCover::Sample GroundCover::sample(float wx, float wz) const
{
    const float fx = wx * _texelsPerUnit - 0.5f;
    const float fy = wz * _texelsPerUnit - 0.5f;
    const int ix = static_cast<int>(std::floor(fx)), iy = static_cast<int>(std::floor(fy));
    const float tx = fx - ix, ty = fy - iy;

    auto at = [&](int x, int y, int c) {
        x = ((x % _w) + _w) % _w;
        y = ((y % _h) + _h) % _h;
        return static_cast<float>(_px[(static_cast<std::size_t>(y) * _w + x) * 4 + c]) / 255.0f;
    };
    auto bil = [&](int c) {
        return (at(ix, iy, c) * (1 - tx) + at(ix + 1, iy, c) * tx) * (1 - ty) +
               (at(ix, iy + 1, c) * (1 - tx) + at(ix + 1, iy + 1, c) * tx) * ty;
    };
    return {bil(0), bil(1), bil(2), bil(3)};
}

bool GroundCover::consumeDirty()
{
    const bool d = _dirty;
    _dirty = false;
    return d;
}
