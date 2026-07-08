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
