#include "interface.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Interface::Interface(std::unique_ptr<NetClient> net, int mapWidth, int mapHeight, int windowWidth, int windowHeight)
    : _engine(windowWidth, windowHeight, std::string(WINDOW_TITLE)), _map(mapWidth, mapHeight), _net(std::move(net))
{
    initCamera();
    // The skybox needs the GL context, so load it after the engine init.
    _engine.loadSkybox("assets/Background.png", "assets/shaders/skybox.vs", "assets/shaders/skybox.fs");
    // Scene shading: per-pixel sun lighting on every model + bloom over the 3D
    // pass (UI drawn after stays crisp). Load before the models so materials
    // pick the shader up at load time; both fail soft to the flat look.
    _engine.loadLightingShader("assets/shaders/lighting.vs", "assets/shaders/lighting.fs");
    _engine.enableBloom("assets/shaders/bloom.fs");
    // Global UI font: every drawText/measureText call goes through it once
    // loaded; on failure the engine silently keeps raylib's built-in font.
    if (!_engine.loadUiFont("assets/toxigenesis bd.otf"))
        gfx::logWarn("loadUiFont: 'assets/toxigenesis bd.otf' failed to load (using built-in font)");
    loadTileTextures();
    loadResourceModels();
    loadPlayerModel();
    loadBackgroundMusic();
    _engine.disableCursor(); // capture the mouse for free-look (Esc releases on close)
}

Interface::~Interface()
{
    // Member destruction runs after this body, so the GL context (owned by
    // _engine) is still alive here — safe to free GPU resources.
    _engine.enableCursor();
    unloadBackgroundMusic();
    unloadPlayerModel();
    unloadResourceModels();
    unloadTileTextures();
    _engine.unloadSkybox();
}

namespace
{
// Index-aligned with MapTile::resources / MAP_RESOURCE_COUNT.
// std::array's size is checked at compile time against MAP_RESOURCE_COUNT below,
// so a mismatch between the resource enum and this list fails to build instead
// of corrupting memory at runtime.
// One mesh per resource, index-aligned with MAP_RESOURCE_NAMES. Food has a
// dedicated chicken mesh; the ore types use monster meshes whose embedded
// texture identifies the mineral.
constexpr std::array<const char *, MAP_RESOURCE_COUNT> kResourceModelPaths = {
    "assets/roast_chicken.glb",      // 0 food
    "assets/monster_black.glb",      // 1 linemate
    "assets/monster_blue.glb",       // 2 deraumere
    "assets/monster_golden.glb",     // 3 sibur
    "assets/monster_green.glb",      // 4 mendiane
    "assets/monster_zero_ultra.glb", // 5 phiras (was monster_lightPink, dup of linemate)
    "assets/monster_pink.glb"        // 6 thystame
};

constexpr std::array<const char *, MAP_RESOURCE_COUNT> kResourceTextureOverrides = {
    "assets/roast_chicken_basecolor.png", // 0 food     — glb base colour is JPEG
    nullptr,                              // 1 linemate — PNG, fine
    nullptr,                              // 2 deraumere— PNG, fine
    nullptr,                              // 3 sibur    — PNG, fine
    "assets/monster_green_basecolor.png", // 4 mendiane — glb base colour is JPEG
    nullptr,                              // 5 phiras   — PNG, fine
    nullptr                               // 6 thystame — PNG, fine
};

// Target display box every resource is scaled into. Upright models (cans) are
// squeezed into the full WIDTH x HEIGHT x WIDTH box per-axis so they all end up
// the SAME size regardless of their native proportions. Flat models (the roast
// chicken) keep their proportions and are scaled by footprint only.
constexpr float kItemWidth = TILE_SIZE * 0.16f;
constexpr float kItemHeight = TILE_SIZE * 0.40f;
constexpr float kFlatWidth = TILE_SIZE * 0.40f; // footprint for flat models (roast chicken)

constexpr const char *kPlayerModelPath = "assets/ai_model3d.glb";
constexpr float kPlayerModelHeight = TILE_SIZE * 0.70f;
constexpr float kPlayerModelFootprint = TILE_SIZE * 0.46f;
constexpr const char *kMusicPath = "assets/music_back.mp3";
constexpr float kMusicVolume = 0.45f;

bool isMusicTogglePressed(RaylibEngine &engine)
{
    if (engine.keyPressed(gfx::Key::M))
        return true;

    int key = engine.charPressed();
    while (key > 0)
    {
        if (key == 'm' || key == 'M')
            return true;
        key = engine.charPressed();
    }
    return false;
}
} // namespace

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void Interface::run()
{
    while (!_engine.shouldClose())
    {
        handleInput();
        update();

        _engine.beginDrawing();
        render();
        _engine.endDrawing();
    }
}

GameMap &Interface::getMap()
{
    return _map;
}
const GameMap &Interface::getMap() const
{
    return _map;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void Interface::loadResourceModels()
{
    _resourceModels.reserve(kResourceModelPaths.size());

    for (size_t i = 0; i < kResourceModelPaths.size(); ++i)
    {
        const char *path = kResourceModelPaths[i];
        ResourceModel rm;

        if (!fs::exists(path))
        {
            gfx::logWarn("loadResourceModels: missing asset '%s' (falling back to cube)", path);
        }
        else
        {
            rm.handle = _engine.loadModel(path);
            if (rm.handle == gfx::NoHandle)
            {
                gfx::logWarn("loadResourceModels: failed to load '%s' (falling back to cube)", path);
            }
            else
            {
                rm.loaded = true;

                // Some meshes embed their base colour as JPEG, which raylib does
                // not decode by default — bind a PNG override so the mesh shows up
                // textured instead of plain white.
                const char *tex = kResourceTextureOverrides[i];
                if (tex != nullptr && fs::exists(tex))
                {
                    gfx::TextureHandle t = _engine.loadTexture(tex);
                    if (t != gfx::NoHandle)
                        _engine.bindModelTexture(rm.handle, t);
                    else
                        gfx::logWarn("loadResourceModels: override texture '%s' failed to load", tex);
                }

                // Re-centre the mesh: different assets have their origin in
                // arbitrary places (off to the side, above the mesh, etc.) Bake a
                // transform so the footprint is centred on the origin and the base
                // rests at y=0 — then every instance can just be dropped at a tile
                // cell + surface height and rotated in place.
                gfx::BBox box = _engine.modelBounds(rm.handle);
                float dx = box.max.x - box.min.x;
                float dy = box.max.y - box.min.y;
                float dz = box.max.z - box.min.z;
                gfx::Vec3 centre = {(box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f};
                _engine.translateModel(rm.handle, {-centre.x, -centre.y, -centre.z});

                const float sx = std::max(dx, 0.0001f);
                const float sy = std::max(dy, 0.0001f);
                const float sz = std::max(dz, 0.0001f);
                const float foot = std::max(sx, sz);
                if (sy >= foot)
                {
                    // Upright object → force into a uniform box so every can is
                    // the same size whatever its native proportions.
                    rm.scale = {kItemWidth / sx, kItemHeight / sy, kItemWidth / sz};
                }
                else
                {
                    // Flat object (roast chicken) → uniform scale on a larger
                    // footprint so it fills the tile instead of looking tiny.
                    const float s = kFlatWidth / foot;
                    rm.scale = {s, s, s};
                }

                gfx::logInfo("RESMODEL %zu '%s' bbox dx=%.2f dy=%.2f dz=%.2f scale=(%.1f,%.1f,%.1f)", i, path, dx, dy,
                             dz, rm.scale.x, rm.scale.y, rm.scale.z);
            }
        }

        _resourceModels.push_back(rm);
    }
}

void Interface::unloadResourceModels()
{
    // unloadModel also frees the material-map textures we bound above.
    for (auto &rm : _resourceModels)
    {
        if (rm.loaded)
            _engine.unloadModel(rm.handle);
    }
    _resourceModels.clear();
}

void Interface::loadPlayerModel()
{
    if (!fs::exists(kPlayerModelPath))
    {
        gfx::logWarn("loadPlayerModel: missing asset '%s' (falling back to cube)", kPlayerModelPath);
        return;
    }

    _playerModel.handle = _engine.loadModel(kPlayerModelPath);
    if (_playerModel.handle == gfx::NoHandle)
    {
        gfx::logWarn("loadPlayerModel: failed to load '%s' (falling back to cube)", kPlayerModelPath);
        return;
    }

    _playerModel.loaded = true;

    gfx::BBox box = _engine.modelBounds(_playerModel.handle);
    const float dx = box.max.x - box.min.x;
    const float dy = box.max.y - box.min.y;
    const float dz = box.max.z - box.min.z;
    const gfx::Vec3 centre = {(box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f};
    _engine.translateModel(_playerModel.handle, {-centre.x, -centre.y, -centre.z});

    const float sx = std::max(dx, 0.0001f);
    const float sy = std::max(dy, 0.0001f);
    const float sz = std::max(dz, 0.0001f);
    const float footprint = std::max(sx, sz);
    const float s = std::min(kPlayerModelHeight / sy, kPlayerModelFootprint / footprint);
    _playerModel.scale = {s, s, s};

    gfx::logInfo("PLAYERMODEL '%s' bbox dx=%.2f dy=%.2f dz=%.2f scale=%.1f", kPlayerModelPath, dx, dy, dz, s);

    loadPlayerAnimations();
}

void Interface::loadPlayerAnimations()
{
    _clipIndex.fill(-1);
    _playerAnims = _engine.loadAnimations(kPlayerModelPath);
    if (_playerAnims == gfx::NoHandle)
    {
        _playerAnimCount = 0;
        gfx::logWarn("loadPlayerAnimations: '%s' has no animations", kPlayerModelPath);
        return;
    }
    _playerAnimCount = _engine.animCount(_playerAnims);

    // Resolve each wanted clip by its name in the .glb. Names are authored in the
    // model; a missing one leaves the slot at -1 and that state simply no-ops.
    const auto bind = [&](PlayerClip clip, const char *name) {
        for (int i = 0; i < _playerAnimCount; ++i)
        {
            if (_engine.animName(_playerAnims, i) == name)
            {
                _clipIndex[static_cast<std::size_t>(clip)] = i;
                return;
            }
        }
        gfx::logWarn("player animation '%s' not found in '%s'", name, kPlayerModelPath);
    };
    bind(PlayerClip::Idle, "Idle");
    bind(PlayerClip::Walk, "Walk");
    bind(PlayerClip::Death, "Death");
    bind(PlayerClip::Kick, "Kick");
    bind(PlayerClip::Dance, "Dance");
    bind(PlayerClip::Pickup, "Pickup");
    bind(PlayerClip::Jump, "Jump");

    for (int i = 0; i < _playerAnimCount; ++i)
        gfx::logInfo("player anim[%d] '%s' frames=%d", i, _engine.animName(_playerAnims, i).c_str(),
                     _engine.animFrameCount(_playerAnims, i));
}

void Interface::unloadPlayerModel()
{
    if (_playerAnims != gfx::NoHandle)
    {
        _engine.unloadAnimations(_playerAnims);
        _playerAnims = gfx::NoHandle;
        _playerAnimCount = 0;
    }
    _clipIndex.fill(-1);
    _playerAnimState.clear();
    _deathGhosts.clear();
    if (_playerModel.loaded)
        _engine.unloadModel(_playerModel.handle);
    _playerModel = {};
}

namespace
{
// raylib 6.x: ModelAnimation.frame is a float interpolated across keyframeCount
// sparse keyframes, so a fixed fps would race short clips and crawl long ones.
// Drive playback by wall-clock duration instead: a clip loops every
// kClipSeconds regardless of how many keyframes it has.
constexpr float kClipSeconds = 1.0f; // seconds for one full clip pass (Idle / Dance)
// The body slides one cell over kMoveDuration at constant speed, and the Walk
// clip is locked to that same progress (see updatePlayerAnimations), so feet
// and ground move together. kStridesPerCell is how many full walk cycles play
// per cell: bump it up/down until the feet look planted instead of skating
// (depends on the clip's built-in stride length).
constexpr float kMoveDuration = 0.5f;   // seconds to walk one cell
constexpr float kStridesPerCell = 1.0f; // walk cycles per cell (foot-slide tuning knob)
constexpr float kPickupSpeed = 6.0f;    // Pickup plays this many times faster (quick snatch)
} // namespace

void Interface::updatePlayerAnimations()
{
    if (!_playerModel.loaded || _playerAnimCount <= 0)
        return;

    const float dt = _engine.frameTime();

    // Make sure every live player has a runtime entry (new players spawn Idle).
    for (const auto &[id, player] : _state.players)
    {
        (void)player;
        _playerAnimState.try_emplace(id);
    }

    // Turn queued protocol events into per-player one-shots / death ghosts.
    for (const auto &ev : _state.animEvents)
    {
        if (ev.kind == PlayerAnimEventKind::Death)
        {
            _deathGhosts.push_back(
                {ev.x * TILE_SIZE + TILE_SIZE / 2.0f, ev.y * TILE_SIZE + TILE_SIZE / 2.0f, ev.orientation, 0.0f});
            _playerAnimState.erase(ev.id);
            continue;
        }
        const PlayerClip clip = ev.kind == PlayerAnimEventKind::Kick     ? PlayerClip::Kick
                                : ev.kind == PlayerAnimEventKind::Pickup ? PlayerClip::Pickup
                                                                         : PlayerClip::Jump;
        PlayerAnimState &st = _playerAnimState[ev.id];
        st.oneShot = clip;
        st.oneShotFrame = 0.0f;
    }
    _state.animEvents.clear();

    // Advance each player's looping state (Idle/Walk/Dance) and any active one-shot.
    for (auto it = _playerAnimState.begin(); it != _playerAnimState.end();)
    {
        const auto pit = _state.players.find(it->first);
        if (pit == _state.players.end())
        { // player gone (e.g. left); drop its state
            it = _playerAnimState.erase(it);
            continue;
        }
        PlayerAnimState &st = it->second;
        const aiPlayer &p = pit->second;

        if (!st.posInit)
        { // first sighting: appear in place
            st.dispX = st.fromX = static_cast<float>(p.getX());
            st.dispY = st.fromY = static_cast<float>(p.getY());
            st.moveProgress = 1.0f;
            st.posInit = true;
        }

        if (p.getX() != st.lastX || p.getY() != st.lastY)
        {
            if (st.lastX != -9999)
            { // skip the glide on first sighting
                const int dx = p.getX() - st.lastX;
                const int dy = p.getY() - st.lastY;
                // One forward step changes a single axis by one cell. Anything
                // bigger is a toroidal wrap or a teleport (fork/respawn): sliding
                // across the map would look worse than a cut, so snap.
                if (dx < -1 || dx > 1 || dy < -1 || dy > 1)
                {
                    st.dispX = st.fromX = static_cast<float>(p.getX());
                    st.dispY = st.fromY = static_cast<float>(p.getY());
                    st.moveProgress = 1.0f;
                }
                else
                {
                    // Start a fresh constant-speed glide from wherever the body is
                    // right now (so a step arriving mid-glide chains cleanly).
                    st.fromX = st.dispX;
                    st.fromY = st.dispY;
                    st.moveProgress = 0.0f;
                }
            }
            st.lastX = p.getX();
            st.lastY = p.getY();
        }

        // Constant-speed linear glide: the ground slides uniformly under the feet
        // (an ease-out would let the legs out-pace the body and skate at the end).
        const float tgtX = static_cast<float>(p.getX());
        const float tgtY = static_cast<float>(p.getY());
        if (st.moveProgress < 1.0f)
        {
            st.moveProgress = std::fmin(1.0f, st.moveProgress + dt / kMoveDuration);
            st.dispX = st.fromX + (tgtX - st.fromX) * st.moveProgress;
            st.dispY = st.fromY + (tgtY - st.fromY) * st.moveProgress;
        }
        else
        {
            st.dispX = tgtX;
            st.dispY = tgtY;
        }

        const bool dancing = _state.incanting.count(static_cast<long long>(p.getY()) * _map.getWidth() + p.getX()) > 0;
        const bool moving = st.moveProgress < 1.0f;

        PlayerClip want = PlayerClip::Idle;
        if (dancing)
            want = PlayerClip::Dance;
        else if (moving)
            want = PlayerClip::Walk;

        if (want != st.loopClip)
        {
            st.loopClip = want;
            st.loopFrame = 0.0f;
        }

        if (const int idx = _clipIndex[static_cast<std::size_t>(st.loopClip)]; idx >= 0)
        {
            const int n = _engine.animFrameCount(_playerAnims, idx);
            if (n > 0)
            {
                if (st.loopClip == PlayerClip::Walk)
                {
                    // Drive the stride straight from the glide progress so the
                    // feet cycle exactly as fast as the body crosses the cell.
                    st.loopFrame =
                        std::fmod(st.moveProgress * kStridesPerCell * static_cast<float>(n), static_cast<float>(n));
                }
                else
                {
                    st.loopFrame = std::fmod(st.loopFrame + dt * n / kClipSeconds, static_cast<float>(n));
                }
            }
        }

        if (st.oneShot != PlayerClip::Count)
        {
            const int idx = _clipIndex[static_cast<std::size_t>(st.oneShot)];
            const int n = idx < 0 ? 0 : _engine.animFrameCount(_playerAnims, idx);
            if (n <= 0)
            {
                st.oneShot = PlayerClip::Count;
            }
            else
            {
                const float speed = st.oneShot == PlayerClip::Pickup ? kPickupSpeed : 1.0f;
                st.oneShotFrame += dt * n / kClipSeconds * speed;
                if (st.oneShotFrame >= n)
                    st.oneShot = PlayerClip::Count; // played once; fall back to the loop clip
            }
        }
        ++it;
    }

    // Advance and retire death ghosts once their clip has played through.
    const int didx = _clipIndex[static_cast<std::size_t>(PlayerClip::Death)];
    const int dn = didx < 0 ? 0 : _engine.animFrameCount(_playerAnims, didx);
    if (dn <= 0)
    {
        _deathGhosts.clear();
    }
    else
    {
        for (auto &g : _deathGhosts)
            g.frame += dt * dn / kClipSeconds;
        _deathGhosts.erase(std::remove_if(_deathGhosts.begin(), _deathGhosts.end(),
                                          [dn](const DeathGhost &g) { return g.frame >= dn; }),
                           _deathGhosts.end());
    }
}

void Interface::applyPlayerPose(std::uint32_t id)
{
    if (_playerAnimCount <= 0)
        return;
    const auto it = _playerAnimState.find(id);
    if (it == _playerAnimState.end())
        return;
    const PlayerAnimState &st = it->second;
    const bool oneShot = st.oneShot != PlayerClip::Count;
    const PlayerClip clip = oneShot ? st.oneShot : st.loopClip;
    const float frame = oneShot ? st.oneShotFrame : st.loopFrame;
    const int idx = _clipIndex[static_cast<std::size_t>(clip)];
    if (idx < 0)
        return;
    _engine.applyPose(_playerModel.handle, _playerAnims, idx, frame);
}

void Interface::loadTileTextures()
{
    _darkTileTexture = _engine.loadTexture("assets/sol_dark.png");
    if (_darkTileTexture == gfx::NoHandle)
        gfx::logWarn("loadTileTextures: failed to load assets/sol_dark.png");

    _orangeTileTexture = _engine.loadTexture("assets/sol_blanc.png");
    if (_orangeTileTexture == gfx::NoHandle)
        gfx::logWarn("loadTileTextures: failed to load assets/sol_blanc.png");
}

void Interface::unloadTileTextures()
{
    if (_darkTileTexture != gfx::NoHandle)
        _engine.unloadTexture(_darkTileTexture);
    if (_orangeTileTexture != gfx::NoHandle)
        _engine.unloadTexture(_orangeTileTexture);
}

void Interface::initCamera()
{
    // Start as a free camera hovering south of the map centre, looking down
    // onto the board — a sensible overview the user then flies away from.
    const float centerX = (_map.getWidth() * TILE_SIZE) / 2.0f;
    const float centerZ = (_map.getHeight() * TILE_SIZE) / 2.0f;
    const float span = std::max(_map.getWidth(), _map.getHeight()) * TILE_SIZE;

    _camera.up = {0.0f, 1.0f, 0.0f};
    _camera.fovy = 60.0f; // a touch wider feels better for a free cam

    _camera.position = {centerX, span * 0.9f, centerZ + span * 0.9f};
    _flySpeed = span * 0.6f; // units/sec; tuned to the map scale

    // Aim at the board centre, then derive yaw/pitch from that direction.
    lookAt({centerX, 0.0f, centerZ});
    updateCameraTarget();
}

void Interface::lookAt(gfx::Vec3 worldTarget)
{
    gfx::Vec3 dir = gfx::sub(worldTarget, _camera.position);
    float horiz = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    _camYaw = std::atan2(dir.x, dir.z);
    _camPitch = std::atan2(dir.y, horiz);
}

void Interface::updateCameraTarget()
{
    // Forward from yaw/pitch; the look target is just eye + forward.
    const float cp = std::cos(_camPitch);
    const gfx::Vec3 forward = {
        cp * std::sin(_camYaw),
        std::sin(_camPitch),
        cp * std::cos(_camYaw),
    };
    _camera.target = gfx::add(_camera.position, forward);
}

void Interface::loadBackgroundMusic()
{
    _audioReady = _engine.initAudio();
    if (!_audioReady)
    {
        gfx::logWarn("loadBackgroundMusic: audio device failed to initialize");
        return;
    }

    if (!fs::exists(kMusicPath))
    {
        gfx::logWarn("loadBackgroundMusic: missing asset '%s'", kMusicPath);
        return;
    }

    _backgroundMusic = _engine.loadMusic(kMusicPath);
    if (_backgroundMusic == gfx::NoHandle)
    {
        gfx::logWarn("loadBackgroundMusic: failed to load '%s'", kMusicPath);
        return;
    }

    _engine.setMusicVolume(_backgroundMusic, kMusicVolume);
    _musicLoaded = true;
    _musicEnabled = true;
    _engine.playMusic(_backgroundMusic);
    gfx::logInfo("loadBackgroundMusic: playing '%s'", kMusicPath);
}

void Interface::unloadBackgroundMusic()
{
    if (_musicLoaded)
    {
        _engine.stopMusic(_backgroundMusic);
        _engine.unloadMusic(_backgroundMusic);
        _musicLoaded = false;
    }
    if (_audioReady)
    {
        _engine.closeAudio();
        _audioReady = false;
    }
}

void Interface::toggleMusic()
{
    if (!_musicLoaded)
        return;

    _musicEnabled = !_musicEnabled;
    if (_musicEnabled)
        _engine.resumeMusic(_backgroundMusic);
    else
        _engine.pauseMusic(_backgroundMusic);
    gfx::logInfo("toggleMusic: music %s", _musicEnabled ? "ON" : "OFF");
}

// ---------------------------------------------------------------------------
// Loop steps
// ---------------------------------------------------------------------------
void Interface::handleInput()
{
    const float dt = _engine.frameTime();

    // ---- Gamepad (first pad, when plugged in) mirrors mouse+keyboard below:
    // sticks fly/look, triggers rise/sink, buttons map onto the key actions.
    const bool pad = _engine.padAvailable();
    const auto padStick = [&](gfx::PadAxis a) {
        const float v = pad ? _engine.padAxis(a) : 0.0f;
        return std::fabs(v) < 0.15f ? 0.0f : v; // stick deadzone
    };

    // ---- Free look: the captured mouse turns the head (yaw/pitch).
    gfx::Vec2 md = _engine.mouseDelta();
    _camYaw -= md.x * 0.0030f + padStick(gfx::PadAxis::RightX) * 2.2f * dt;
    _camPitch -= md.y * 0.0030f + padStick(gfx::PadAxis::RightY) * 2.2f * dt;
    // Clamp pitch just shy of straight up/down so the view never flips.
    const float limit = 1.553f; // ~89deg
    if (_camPitch > limit)
        _camPitch = limit;
    if (_camPitch < -limit)
        _camPitch = -limit;

    // ---- Movement basis from the current heading.
    const float cp = std::cos(_camPitch);
    const gfx::Vec3 forward = {cp * std::sin(_camYaw), std::sin(_camPitch), cp * std::cos(_camYaw)};
    const gfx::Vec3 rightH = {-std::cos(_camYaw), 0.0f, std::sin(_camYaw)}; // strafe stays level

    const float boost = (_engine.keyDown(gfx::Key::LeftShift) || _engine.keyDown(gfx::Key::RightShift) ||
                         (pad && _engine.padDown(gfx::PadBtn::LeftThumb)))
                            ? 3.0f
                            : 1.0f;
    const float step = _flySpeed * dt * boost;
    gfx::Vec3 move{0.0f, 0.0f, 0.0f};

    if (_engine.keyDown(gfx::Key::W) || _engine.keyDown(gfx::Key::Up))
        move = gfx::add(move, forward);
    if (_engine.keyDown(gfx::Key::S) || _engine.keyDown(gfx::Key::Down))
        move = gfx::sub(move, forward);
    if (_engine.keyDown(gfx::Key::D) || _engine.keyDown(gfx::Key::Right))
        move = gfx::add(move, rightH);
    if (_engine.keyDown(gfx::Key::A) || _engine.keyDown(gfx::Key::Left))
        move = gfx::sub(move, rightH);
    if (_engine.keyDown(gfx::Key::Space))
        move.y += 1.0f; // ascend
    if (_engine.keyDown(gfx::Key::LeftControl) || _engine.keyDown(gfx::Key::C))
        move.y -= 1.0f; // descend

    // Left stick flies analog (stick up = forward); triggers rise (RT) / sink (LT).
    move = gfx::add(move, gfx::scale(forward, -padStick(gfx::PadAxis::LeftY)));
    move = gfx::add(move, gfx::scale(rightH, padStick(gfx::PadAxis::LeftX)));
    if (pad)
    {
        // raylib triggers rest at -1; remap to 0..1 before use.
        const float rt = (_engine.padAxis(gfx::PadAxis::RightTrigger) + 1.0f) * 0.5f;
        const float lt = (_engine.padAxis(gfx::PadAxis::LeftTrigger) + 1.0f) * 0.5f;
        if (rt > 0.05f)
            move.y += rt;
        if (lt > 0.05f)
            move.y -= lt;
    }

    if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f)
    {
        // Clamp (not normalize) so partial stick deflection stays analog while
        // keyboard diagonals still cap at the same speed as a single key.
        const float len = gfx::length(move);
        move = gfx::scale(move, step * std::min(len, 1.0f) / len);
        _camera.position = gfx::add(_camera.position, move);
    }

    // ---- Enter dismisses (or brings back) the end screen once there's a winner,
    // so the spectator can keep flying around the finished game normally.
    if (_state.hasWinner && (_engine.keyPressed(gfx::Key::Enter) || _engine.padPressed(gfx::PadBtn::FaceRight)))
        _endHidden = !_endHidden;

    // ---- Wheel sets the fly speed (not zoom — there's no pivot to zoom to).
    // While the end screen is up, the same wheel scrolls the winner report instead.
    const bool endScreenUp = _state.hasWinner && !_endHidden;
    float wheel = _engine.mouseWheel();
    if (pad)
    {
        // Held bumpers act as a continuous wheel (~4 notches per second).
        const float bump = (_engine.padDown(gfx::PadBtn::RightBumper) ? 1.0f : 0.0f) -
                           (_engine.padDown(gfx::PadBtn::LeftBumper) ? 1.0f : 0.0f);
        wheel += bump * 4.0f * dt;
    }
    if (wheel != 0.0f && endScreenUp)
    {
        _endScroll -= wheel * 56.0f;
        if (_endScroll < 0.0f)
            _endScroll = 0.0f;
    }
    else if (wheel != 0.0f)
    {
        _flySpeed *= (1.0f + wheel * 0.12f);
        const float span = std::max(_map.getWidth(), _map.getHeight()) * TILE_SIZE;
        const float minS = span * 0.05f, maxS = span * 4.0f;
        if (_flySpeed < minS)
            _flySpeed = minS;
        if (_flySpeed > maxS)
            _flySpeed = maxS;
    }

    // ---- R: snap back to the overview (also drops follow).
    if (_engine.keyPressed(gfx::Key::R) || _engine.padPressed(gfx::PadBtn::FaceLeft))
    {
        _followedPlayer = -1;
        initCamera();
    }

    // ---- F: toggle riding along with the selected player.
    if (_engine.keyPressed(gfx::Key::F) || _engine.padPressed(gfx::PadBtn::FaceUp))
    {
        if (_followedPlayer >= 0)
        {
            _followedPlayer = -1;
        }
        else if (_selectedX >= 0 && _selectedY >= 0)
        {
            const MapTile &tile = _map.getTile(_selectedX, _selectedY);
            if (!tile.players.empty())
            {
                _followedPlayer = static_cast<std::int64_t>(tile.players.front().getId());
                _followAnchor = {_selectedX * TILE_SIZE + TILE_SIZE / 2.0f, 0.0f,
                                 _selectedY * TILE_SIZE + TILE_SIZE / 2.0f};
            }
        }
    }

    // ---- Simulation speed: +/- request a new time unit from the server (sst).
    if (_engine.keyPressed(gfx::Key::Equal) || _engine.keyPressed(gfx::Key::KpAdd) ||
        _engine.padPressed(gfx::PadBtn::DpadUp))
        requestTimeUnit(_desiredFreq + (_desiredFreq < 10 ? 1 : 10));
    if (_engine.keyPressed(gfx::Key::Minus) || _engine.keyPressed(gfx::Key::KpSubtract) ||
        _engine.padPressed(gfx::PadBtn::DpadDown))
        requestTimeUnit(_desiredFreq - (_desiredFreq <= 10 ? 1 : 10));

    // ---- Timeline: pause, scrub back/forward through recorded history, go live.
    if (_engine.keyPressed(gfx::Key::P) || _engine.padPressed(gfx::PadBtn::Start))
        togglePause();
    if (_engine.keyPressed(gfx::Key::PageDown) || _engine.padPressed(gfx::PadBtn::DpadLeft))
        scrubBy(-1.0f); // back in time
    if (_engine.keyPressed(gfx::Key::PageUp) || _engine.padPressed(gfx::PadBtn::DpadRight))
        scrubBy(+1.0f); // forward in time
    if (_engine.keyPressed(gfx::Key::End))
        goLive();

    // ---- Panels / music.
    if (_engine.keyPressed(gfx::Key::Tab) || _engine.padPressed(gfx::PadBtn::Select))
        _showStats = !_showStats;
    if (_engine.keyPressed(gfx::Key::H) || _engine.keyPressed(gfx::Key::F1) ||
        _engine.padPressed(gfx::PadBtn::RightThumb))
        _showHelp = !_showHelp;
    if (isMusicTogglePressed(_engine))
        toggleMusic();

    // ---- Left-click / A button (crosshair): select the aimed tile; double-click focuses.
    if (_engine.mousePressed(gfx::MouseBtn::Left) || _engine.padPressed(gfx::PadBtn::FaceDown))
    {
        pickTile();
        const double now = _engine.time();
        if (_lastClickTime >= 0.0 && (now - _lastClickTime) < 0.30 && _selectedX >= 0 && _selectedY >= 0)
        {
            focusOnTile(_selectedX, _selectedY);
        }
        _lastClickTime = now;
    }

    updateCameraTarget();
}

void Interface::focusOnTile(int tx, int ty)
{
    // Fly to a fixed vantage above/south of the tile and aim down at it.
    _followedPlayer = -1;
    const gfx::Vec3 centre = {tx * TILE_SIZE + TILE_SIZE / 2.0f, 0.0f, ty * TILE_SIZE + TILE_SIZE / 2.0f};
    _camera.position = {centre.x, TILE_SIZE * 5.0f, centre.z + TILE_SIZE * 5.0f};
    lookAt(centre);
    updateCameraTarget();
}

void Interface::requestTimeUnit(int freq)
{
    if (freq < 1)
        freq = 1; // the server rejects T <= 0 with sbp
    _desiredFreq = freq;
    if (_net && !_net->closed())
        _net->send("sst " + std::to_string(freq)); // server acks with "sst T"
}

// ---------------------------------------------------------------------------
// Timeline (record + scrub)
// ---------------------------------------------------------------------------
void Interface::recordIncoming()
{
    if (!_net)
        return;
    // Always record; the socket must be drained even while paused so it does
    // not back up. In live mode the line is also applied to the world now.
    for (auto &line : _net->poll())
    {
        _history.push_back({_elapsed, line});
        if (_live)
        {
            _parser.apply(line, _map, _state);
            _appliedIndex = _history.size();
        }
    }
    if (_live)
        _playT = latestTime();
}

void Interface::rebuildWorldTo(float t)
{
    // Reconstruct the world at time t by replaying every recorded line up to it
    // onto a fresh map/state. O(history); only called on a user scrub action.
    const int w = _map.getWidth();
    const int h = _map.getHeight();
    _map = GameMap(w, h);
    _state = GuiState{};
    for (const auto &rec : _history)
    {
        if (rec.t <= t)
            _parser.apply(rec.line, _map, _state);
        else
            break; // _history is in arrival order, so the rest is in the future
    }
    // The replay re-fired every historical action packet; drop the queued one-shots
    // so a scrub doesn't trigger a burst of kicks/jumps/deaths on the next frame.
    // Same for the narrative feed/bubbles: the log keeps what already scrolled by,
    // but replayed events must not re-announce themselves.
    _state.animEvents.clear();
    _state.feedEvents.clear();
    _bubbles.clear();
    _deathGhosts.clear();
    _playerAnimState.clear();
    _playT = t;
}

void Interface::togglePause()
{
    if (_live)
    {
        _live = false; // freeze on the current (latest) instant
        _playT = latestTime();
    }
    else
    {
        goLive(); // resume following the stream
    }
}

void Interface::scrubBy(float seconds)
{
    if (_history.empty())
        return;
    if (_live)
        _live = false; // first scrub drops out of live
    float t = _playT + seconds;
    if (t < 0.0f)
        t = 0.0f;
    if (t > latestTime())
        t = latestTime();
    rebuildWorldTo(t);
}

void Interface::goLive()
{
    // Catch the world up to everything recorded, then resume incremental apply.
    rebuildWorldTo(latestTime());
    _appliedIndex = _history.size();
    _playT = latestTime();
    _live = true;
}

void Interface::update()
{
    if (_musicLoaded)
        _engine.updateMusic(_backgroundMusic);

    _elapsed += _engine.frameTime();

    // Record whatever the server pushed since last frame; in live mode it is
    // also folded into the world right away. Scrubbing replays from _history.
    recordIncoming();

    // Turn queued narrative events into feed lines + speech bubbles.
    drainFeedEvents();

    // Track the tile under the crosshair for the hover outline + tooltip.
    if (!aimedTile(_hoverX, _hoverY))
        _hoverX = _hoverY = -1;

    // Seed the speed control from the server's first reported time unit (sgt),
    // so +/- nudge from the real value instead of from zero.
    if (!_freqInit && _state.frequency > 0)
    {
        _desiredFreq = _state.frequency;
        _freqInit = true;
    }

    // Camera ride-along: translate the eye by however far the followed player
    // moved since last frame, so free-look and free-fly still work while the
    // camera tracks it. Drop the follow silently if the player died/left.
    if (_followedPlayer >= 0)
    {
        auto it = _state.players.find(static_cast<std::uint32_t>(_followedPlayer));
        if (it == _state.players.end())
        {
            _followedPlayer = -1;
        }
        else
        {
            // Track the smoothed display position (falls back to the tile when no
            // anim state yet) so the camera glides with the body instead of
            // jumping a full cell each time the followed player steps.
            float fx = static_cast<float>(it->second.getX());
            float fy = static_cast<float>(it->second.getY());
            if (const auto ait = _playerAnimState.find(static_cast<std::uint32_t>(_followedPlayer));
                ait != _playerAnimState.end() && ait->second.posInit)
            {
                fx = ait->second.dispX;
                fy = ait->second.dispY;
            }
            const gfx::Vec3 now = {fx * TILE_SIZE + TILE_SIZE / 2.0f, 0.0f, fy * TILE_SIZE + TILE_SIZE / 2.0f};
            _camera.position = gfx::add(_camera.position, gfx::sub(now, _followAnchor));
            _followAnchor = now;
            updateCameraTarget();
        }
    }

    // Drive per-player animation state (walk/idle/dance + one-shots + death ghosts).
    updatePlayerAnimations();
}

namespace
{
constexpr float kTileHeight = 2.0f;
constexpr float kTileMargin = 0.0f;
constexpr float kPlayerBaseSize = TILE_SIZE * 0.4f;
constexpr float kPlayerY = 4.0f;
constexpr float kPlayerHeight = 6.0f;
constexpr float kTileTopY = kTileHeight / 2.0f;   // surface items stand on
constexpr float kItemSpacing = TILE_SIZE * 0.22f; // grid pitch between stacked items
constexpr int kMaxVisiblePlayers = 4;
constexpr float kPlayerSpacing = TILE_SIZE * 0.28f;

struct CountLabel
{
    gfx::Vec3 worldPos;
    int count;
    gfx::Color color;
};

// Eight fixed team colours, indexed by team slot (server `tna` order). The
// game caps at 8 teams, so every team gets its own distinct hue regardless
// of the team name.
constexpr std::array<gfx::Color, 8> kTeamPalette{{
    {230, 60, 70, 255},   // 0 red
    {70, 155, 255, 255},  // 1 blue
    {60, 210, 120, 255},  // 2 green
    {255, 210, 55, 255},  // 3 yellow
    {180, 105, 255, 255}, // 4 purple
    {255, 145, 55, 255},  // 5 orange
    {60, 220, 220, 255},  // 6 cyan
    {255, 105, 180, 255}, // 7 pink
}};

// Lighten a team colour toward white so a model tinted with it keeps its
// texture detail and merely picks up a coloured sheen (a subtle glow),
// instead of being flatly repainted.
gfx::Color glowTint(gfx::Color c)
{
    auto mix = [](std::uint8_t v) { return static_cast<std::uint8_t>(v + (255 - v) * 0.45f); };
    return gfx::Color{mix(c.r), mix(c.g), mix(c.b), 255};
}

const char *orientationLabel(Orientation orientation)
{
    switch (orientation)
    {
    case Orientation::North:
        return "N";
    case Orientation::East:
        return "E";
    case Orientation::South:
        return "S";
    case Orientation::West:
        return "W";
    }
    return "?";
}

void drawTileOutline(RaylibEngine &engine, float worldX, float worldZ, float size)
{
    const float half = size * 0.5f;
    const float y = kTileTopY + 0.02f;

    engine.drawLine3D({worldX - half, y, worldZ - half}, {worldX + half, y, worldZ - half}, gfx::BLACK);
    engine.drawLine3D({worldX + half, y, worldZ - half}, {worldX + half, y, worldZ + half}, gfx::BLACK);
    engine.drawLine3D({worldX + half, y, worldZ + half}, {worldX - half, y, worldZ + half}, gfx::BLACK);
    engine.drawLine3D({worldX - half, y, worldZ + half}, {worldX - half, y, worldZ - half}, gfx::BLACK);
}

float playerOrientationAngle(Orientation orientation)
{
    switch (orientation)
    {
    case Orientation::North:
        return 180.0f;
    case Orientation::East:
        return 90.0f;
    case Orientation::South:
        return 0.0f;
    case Orientation::West:
        return 270.0f;
    }
    return 0.0f;
}
} // namespace

void Interface::render()
{
    std::vector<CountLabel> labels;

    _engine.beginMode3D(_camera);

    // 360 background first, so the rest of the scene draws in front of it.
    _engine.drawSkybox();

    // Floor: two batched draws (dark + orange) instead of one per tile.
    const bool floorTextured = _darkTileTexture != gfx::NoHandle || _orangeTileTexture != gfx::NoHandle;
    if (floorTextured)
    {
        _engine.drawCheckerFloor(_darkTileTexture, _orangeTileTexture, _map.getWidth(), _map.getHeight(), TILE_SIZE,
                                 TILE_SIZE - kTileMargin, kTileTopY);
    }

    // Frustum cull: skip the expensive per-tile content (outlines, players,
    // resource models) for tiles whose centre projects off-screen. The batched
    // floor above stays fully drawn — it's cheap and avoids holes at the edges.
    const gfx::Vec3 camFwd = gfx::normalize(gfx::sub(_camera.target, _camera.position));
    const float screenW = static_cast<float>(_engine.screenWidth());
    const float screenH = static_cast<float>(_engine.screenHeight());
    const float cullMargin = 160.0f; // px slack so partly-visible edge tiles still draw
    auto tileVisible = [&](float wx, float wz) {
        gfx::Vec3 c{wx, kTileTopY, wz};
        gfx::Vec3 d = gfx::sub(c, _camera.position);
        if (gfx::dot(d, camFwd) <= 0.0f)
            return false; // behind the camera
        gfx::Vec2 sp = _engine.worldToScreen(_camera, c);
        return sp.x >= -cullMargin && sp.x <= screenW + cullMargin && sp.y >= -cullMargin &&
               sp.y <= screenH + cullMargin;
    };

    for (int y = 0; y < _map.getHeight(); ++y)
    {
        for (int x = 0; x < _map.getWidth(); ++x)
        {
            float worldX = x * TILE_SIZE + TILE_SIZE / 2.0f;
            float worldZ = y * TILE_SIZE + TILE_SIZE / 2.0f;

            if (!tileVisible(worldX, worldZ))
                continue;

            const MapTile &tile = _map.getTile(x, y);

            // Untextured fallback only (textured floor was drawn batched above).
            if (!floorTextured)
                _engine.drawPlane({worldX, kTileTopY, worldZ}, {TILE_SIZE - kTileMargin, TILE_SIZE - kTileMargin},
                                  gfx::WHITE);
            drawTileOutline(_engine, worldX, worldZ, TILE_SIZE - kTileMargin);

            // Players: draw up to four robots side by side, then show a count label.
            const int playerCount = static_cast<int>(tile.players.size());
            if (playerCount > 0)
            {
                const int visiblePlayers = std::min(playerCount, kMaxVisiblePlayers);
                const int cols = visiblePlayers == 1 ? 1 : 2;
                const int rows = (visiblePlayers + cols - 1) / cols;
                const float originX = worldX - kPlayerSpacing * static_cast<float>(cols - 1) * 0.5f;
                const float originZ = worldZ - kPlayerSpacing * static_cast<float>(rows - 1) * 0.5f;

                for (int i = 0; i < visiblePlayers; ++i)
                {
                    const aiPlayer &player = tile.players[static_cast<size_t>(i)];
                    const int col = i % cols;
                    const int row = i / cols;
                    float px = originX + kPlayerSpacing * static_cast<float>(col);
                    float pz = originZ + kPlayerSpacing * static_cast<float>(row);

                    // The animator advances dispX/dispY behind the logical tile
                    // while a step is in flight; shift the model by that lag so it
                    // slides in from the previous cell instead of popping into this
                    // one. When settled (disp == tile) the offset is zero.
                    if (const auto ait = _playerAnimState.find(player.getId()); ait != _playerAnimState.end())
                    {
                        px += (ait->second.dispX - static_cast<float>(x)) * TILE_SIZE;
                        pz += (ait->second.dispY - static_cast<float>(y)) * TILE_SIZE;
                    }

                    // Tint each robot with its team's colour (lightened to a sheen)
                    // so teams are tellable apart at a glance.
                    const gfx::Color teamGlow = glowTint(teamColor(player.getTeam()));
                    if (_playerModel.loaded)
                    {
                        // Upload this player's own clip+frame before drawing it, so
                        // each robot shows its own animation despite sharing one model.
                        applyPlayerPose(player.getId());
                        _engine.drawModelEx(_playerModel.handle, {px, kTileTopY, pz}, {0.0f, 1.0f, 0.0f},
                                            playerOrientationAngle(player.getOrientation()), _playerModel.scale,
                                            teamGlow);
                    }
                    else
                    {
                        _engine.drawCube({px, kPlayerY, pz}, kPlayerBaseSize, kPlayerHeight, kPlayerBaseSize,
                                         teamColor(player.getTeam()));
                    }
                }
                if (playerCount > kMaxVisiblePlayers)
                    labels.push_back({{worldX, kPlayerY + kPlayerHeight, worldZ}, playerCount, gfx::YELLOW});
            }

            // Resources render only on empty tiles; occupied tiles show the player model alone.
            int totalItems = 0;
            if (playerCount == 0)
            {
                for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
                    totalItems += tile.resources[i];
            }

            if (playerCount == 0 && totalItems > 0)
            {
                const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(totalItems))));
                const int rows = (totalItems + cols - 1) / cols;
                // Shrink the pitch if the natural grid would spill past the tile,
                // so items stay on their own tile no matter the count.
                const float maxSpan = TILE_SIZE - kTileMargin;
                const float pitch = std::min(kItemSpacing, maxSpan / static_cast<float>(std::max(cols, rows)));
                const float originX = worldX - pitch * (cols - 1) * 0.5f;
                const float originZ = worldZ - pitch * (rows - 1) * 0.5f;

                int slot = 0;
                for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
                {
                    const int count = tile.resources[i];
                    const bool hasModel = static_cast<size_t>(i) < _resourceModels.size() && _resourceModels[i].loaded;

                    for (int n = 0; n < count; ++n, ++slot)
                    {
                        const int col = slot % cols;
                        const int row = slot / cols;
                        const float cx = originX + pitch * static_cast<float>(col);
                        const float cz = originZ + pitch * static_cast<float>(row);
                        const float angle = static_cast<float>((x * 53 + y * 97 + i * 17 + slot * 31) % 360);

                        if (hasModel)
                        {
                            const ResourceModel &rm = _resourceModels[i];
                            _engine.drawModelEx(rm.handle, {cx, kTileTopY, cz}, {0.0f, 1.0f, 0.0f}, angle, rm.scale,
                                                gfx::WHITE);
                        }
                        else
                        {
                            const float s = kItemWidth;
                            _engine.drawCube({cx, kTileTopY + s / 2.0f, cz}, s, s, s, gfx::RED);
                        }
                    }
                }
            }
        }
    }

    // Eggs: small cream spheres resting on the tile surface.
    for (const auto &[id, egg] : _state.eggs)
    {
        (void)id;
        if (egg.x < 0 || egg.y < 0 || egg.x >= _map.getWidth() || egg.y >= _map.getHeight())
            continue;
        float ex = egg.x * TILE_SIZE + TILE_SIZE / 2.0f;
        float ez = egg.y * TILE_SIZE + TILE_SIZE / 2.0f;
        if (!tileVisible(ex, ez))
            continue;
        _engine.drawSphere({ex, kTileTopY + TILE_SIZE * 0.08f, ez}, TILE_SIZE * 0.08f, gfx::BEIGE);
    }

    // Death ghosts: players already erased from the world, replaying the Death clip
    // once at the spot they fell so the death reads on screen.
    if (_playerModel.loaded && !_deathGhosts.empty())
    {
        const int didx = _clipIndex[static_cast<std::size_t>(PlayerClip::Death)];
        if (didx >= 0)
        {
            for (const auto &g : _deathGhosts)
            {
                if (!tileVisible(g.x, g.y))
                    continue;
                _engine.applyPose(_playerModel.handle, _playerAnims, didx, g.frame);
                _engine.drawModelEx(_playerModel.handle, {g.x, kTileTopY, g.y}, {0.0f, 1.0f, 0.0f},
                                    playerOrientationAngle(g.orientation), _playerModel.scale, gfx::WHITE);
            }
        }
    }

    drawIncantationRings();
    drawHoverHighlight();
    drawSelectionHighlight();

    _engine.endMode3D();

    // Player head-count labels, projected to screen space now that we're out
    // of 3D mode. Resources are always drawn in full, so they need no label.
    for (const auto &label : labels)
    {
        gfx::Vec2 screenPos = _engine.worldToScreen(_camera, label.worldPos);
        _engine.drawText(gfx::fmt("x%d", label.count), static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), 14,
                         label.color);
    }

    drawSpeechBubbles();
    drawHud();
    drawTimeline();
    drawEventFeed();
    drawTileInfoPanel();
    drawHoverTooltip();
    if (_showStats)
        drawStatsPanel();
    if (_showHelp)
        drawHelpOverlay();
    if (_state.hasWinner && !_endHidden)
        drawEndScreen();

    _engine.drawFps(_engine.screenWidth() - 90, 10);
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------
bool Interface::aimedTile(int &tx, int &ty) const
{
    // The cursor is captured (centred) in free-cam, so aim from the crosshair
    // at the middle of the screen rather than the frozen mouse position.
    gfx::Vec2 centre = {_engine.screenWidth() / 2.0f, _engine.screenHeight() / 2.0f};
    gfx::Ray ray = _engine.screenToWorldRay(_camera, centre);
    if (std::fabs(ray.direction.y) < 1e-6f)
        return false; // ray parallel to the ground, no hit

    // Intersect the tile-top plane y = kTileTopY.
    float t = (kTileTopY - ray.position.y) / ray.direction.y;
    if (t < 0.0f)
        return false; // plane is behind the camera

    float hx = ray.position.x + ray.direction.x * t;
    float hz = ray.position.z + ray.direction.z * t;
    int x = static_cast<int>(std::floor(hx / TILE_SIZE));
    int y = static_cast<int>(std::floor(hz / TILE_SIZE));

    if (x < 0 || y < 0 || x >= _map.getWidth() || y >= _map.getHeight())
        return false; // aiming off the board

    tx = x;
    ty = y;
    return true;
}

void Interface::pickTile()
{
    int tx = -1, ty = -1;
    if (aimedTile(tx, ty))
    {
        _selectedX = tx;
        _selectedY = ty;
    }
    else
    {
        _selectedX = _selectedY = -1; // clicked off the board -> deselect
    }
}

void Interface::drawIncantationRings()
{
    if (_state.incanting.empty())
        return;

    // Three expanding, fading rings per ritual tile plus a pulsing core disc.
    // The colours sit above the bloom threshold, so the whole thing glows.
    constexpr int kRings = 3;
    const gfx::Color kRitual{210, 130, 255, 255};
    const float y = kTileTopY + 0.25f;
    const float maxR = TILE_SIZE * 0.46f;

    for (const long long key : _state.incanting)
    {
        const int tx = static_cast<int>(key % _map.getWidth());
        const int ty = static_cast<int>(key / _map.getWidth());
        const float wx = tx * TILE_SIZE + TILE_SIZE / 2.0f;
        const float wz = ty * TILE_SIZE + TILE_SIZE / 2.0f;

        for (int i = 0; i < kRings; ++i)
        {
            // Phase-offset sawtooth: each ring grows 0 -> maxR then wraps.
            float phase = _elapsed * 0.7f + static_cast<float>(i) / kRings;
            phase -= std::floor(phase);
            const float r = maxR * phase;
            const auto alpha = static_cast<std::uint8_t>(230.0f * (1.0f - phase));
            _engine.drawCircle3D({wx, y, wz}, r, gfx::Color{kRitual.r, kRitual.g, kRitual.b, alpha});
        }

        // Core pulse: a slim bright disc the bloom pass turns into a glow.
        const float pulse = 0.5f + 0.5f * std::sin(_elapsed * 6.0f);
        const auto coreA = static_cast<std::uint8_t>(90.0f + 90.0f * pulse);
        _engine.drawCube({wx, y, wz}, TILE_SIZE * 0.5f, 0.2f, TILE_SIZE * 0.5f,
                         gfx::Color{kRitual.r, kRitual.g, kRitual.b, coreA});
    }
}

void Interface::drawSelectionHighlight()
{
    if (_selectedX < 0 || _selectedY < 0)
        return;

    float wx = _selectedX * TILE_SIZE + TILE_SIZE / 2.0f;
    float wz = _selectedY * TILE_SIZE + TILE_SIZE / 2.0f;
    float size = TILE_SIZE - kTileMargin;
    float half = size * 0.5f;

    // Translucent overlay + bright gold border just above the tile surface.
    // The fill breathes slowly so the selection reads as "active", not painted.
    const float breathe = 0.5f + 0.5f * std::sin(_elapsed * 3.0f);
    const auto fillA = static_cast<std::uint8_t>(45.0f + 45.0f * breathe);
    _engine.drawCube({wx, kTileTopY + 0.15f, wz}, size, 0.3f, size, gfx::Color{255, 230, 0, fillA});

    float y = kTileTopY + 0.35f;
    gfx::Vec3 a = {wx - half, y, wz - half};
    gfx::Vec3 b = {wx + half, y, wz - half};
    gfx::Vec3 c = {wx + half, y, wz + half};
    gfx::Vec3 d = {wx - half, y, wz + half};
    _engine.drawLine3D(a, b, gfx::GOLD);
    _engine.drawLine3D(b, c, gfx::GOLD);
    _engine.drawLine3D(c, d, gfx::GOLD);
    _engine.drawLine3D(d, a, gfx::GOLD);
}

void Interface::drawTileInfoPanel()
{
    if (_selectedX < 0 || _selectedY < 0)
        return;

    const MapTile &tile = _map.getTile(_selectedX, _selectedY);

    // Build the lines first so the panel can size itself.
    std::vector<std::pair<std::string, gfx::Color>> lines;
    lines.push_back({gfx::fmt("Tile (%d, %d)", _selectedX, _selectedY), gfx::GOLD});

    int totalRes = 0;
    for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
    {
        int q = tile.resources[i];
        totalRes += q;
        if (q > 0)
            lines.push_back({gfx::fmt("%-9s %d", std::string(MAP_RESOURCE_NAMES[i]).c_str(), q), gfx::RAYWHITE});
    }
    if (totalRes == 0)
        lines.push_back({"(no resources)", gfx::GRAY});

    // Players standing on the tile, enriched with level/team from the registry.
    lines.push_back({gfx::fmt("Players: %d", static_cast<int>(tile.players.size())), gfx::SKYBLUE});
    for (const aiPlayer &player : tile.players)
    {
        lines.push_back({gfx::fmt("  #%u  lvl %d  %s", player.getId(), player.getLevel(), player.getTeam().c_str()),
                         gfx::LIGHTGRAY});
    }

    // Eggs on this tile.
    int eggs = 0;
    for (const auto &[eid, egg] : _state.eggs)
    {
        (void)eid;
        if (egg.x == _selectedX && egg.y == _selectedY)
            ++eggs;
    }
    if (eggs > 0)
        lines.push_back({gfx::fmt("Eggs: %d", eggs), gfx::BEIGE});

    // Panel geometry: top-right.
    const int pad = 10;
    const int lineH = 18;
    const int width = 220;
    const int height = pad * 2 + static_cast<int>(lines.size()) * lineH;
    const int px = _engine.screenWidth() - width - 10;
    const int py = 120;

    _engine.drawRect(px, py, width, height, gfx::Color{0, 0, 0, 180});
    _engine.drawRectLines(px, py, width, height, gfx::GOLD);

    int ty = py + pad;
    for (const auto &[text, color] : lines)
    {
        _engine.drawText(text, px + pad, ty, 14, color);
        ty += lineH;
    }
}

// ---------------------------------------------------------------------------
// Event feed / speech bubbles / hover
// ---------------------------------------------------------------------------
namespace
{
constexpr std::size_t kFeedKeep = 60;  // entries retained in the deque
constexpr int kFeedVisible = 9;        // lines drawn at once
constexpr float kFeedFadeStart = 8.0f; // seconds fully opaque
constexpr float kFeedFadeEnd = 14.0f;  // seconds until fully gone
constexpr float kBubbleSeconds = 4.0f; // how long a broadcast bubble lingers
constexpr std::size_t kBubbleMaxChars = 48;
constexpr std::size_t kFeedMaxChars = 64;

std::string truncated(std::string s, std::size_t maxChars)
{
    if (s.size() > maxChars)
    {
        s.resize(maxChars - 3);
        s += "...";
    }
    return s;
}

// The baseline AI coordinates over "TEAM:TYPE:key=value:..." packets; that's
// wire code, not something a spectator should read. Translate the known
// intents into plain speech and leave anything unrecognized (other AIs,
// actual free-text chat) untouched.
std::string prettyBroadcast(const std::string &raw)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= raw.size())
    {
        const std::size_t sep = raw.find(':', start);
        if (sep == std::string::npos)
        {
            parts.push_back(raw.substr(start));
            break;
        }
        parts.push_back(raw.substr(start, sep - start));
        start = sep + 1;
    }
    if (parts.size() < 2)
        return raw;

    const std::string &type = parts[1];
    const auto field = [&](const char *key) -> std::string {
        for (std::size_t i = 2; i < parts.size(); ++i)
        {
            const std::size_t eq = parts[i].find('=');
            if (eq != std::string::npos && parts[i].compare(0, eq, key) == 0)
                return parts[i].substr(eq + 1);
        }
        return {};
    };

    if (type == "HELLO")
        return "Hello !";
    if (type == "GATHER")
    {
        const std::string level = field("level");
        const std::string need = field("need");
        std::string msg = "Rally to me";
        if (!level.empty())
            msg += " for the level " + level + " ritual";
        if (!need.empty())
            msg += " - need " + need + " more";
        return msg + "!";
    }
    if (type == "GATHER_ACK")
        return "On my way!";
    if (type == "HOLD")
        return "Hold the tile - ritual is forming!";
    if (type == "ARRIVED")
        return "I'm here for the ritual!";
    if (type == "MCOME")
        return "Everyone converge on me!";
    if (type == "MFOOD")
        return "Stockpile food!";
    if (type == "MRDY")
        return "I'm ready!";
    if (type == "MRDY2")
        return "Ready confirmed - GO!";

    return raw; // unknown scheme: show what was actually said
}
} // namespace

void Interface::drainFeedEvents()
{
    for (const auto &ev : _state.feedEvents)
    {
        std::string text;
        gfx::Color color = gfx::LIGHTGRAY;
        switch (ev.kind)
        {
        case GameEventKind::Join:
            text = gfx::fmt("#%u joined %s (lvl %d)", ev.id, ev.text.c_str(), ev.value);
            color = gfx::LIGHTGRAY;
            break;
        case GameEventKind::Broadcast: {
            const std::string spoken = prettyBroadcast(ev.text);
            text = gfx::fmt("#%u: %s", ev.id, truncated(spoken, kFeedMaxChars).c_str());
            color = gfx::SKYBLUE;
            _bubbles[ev.id] = {truncated(spoken, kBubbleMaxChars), _elapsed + kBubbleSeconds};
            break;
        }
        case GameEventKind::Death:
            text = gfx::fmt("#%u (%s) died", ev.id, ev.text.c_str());
            color = gfx::RED;
            break;
        case GameEventKind::LevelUp:
            text = gfx::fmt("#%u (%s) reached level %d", ev.id, ev.text.c_str(), ev.value);
            color = gfx::GOLD;
            break;
        case GameEventKind::IncantStart:
            text = gfx::fmt("Level %d ritual started at (%d,%d)", ev.value, ev.x, ev.y);
            color = gfx::Color{180, 105, 255, 255};
            break;
        case GameEventKind::IncantEnd:
            if (ev.value == 0)
            {
                text = gfx::fmt("Ritual at (%d,%d) FAILED", ev.x, ev.y);
                color = gfx::Color{255, 120, 120, 255};
            }
            else
            {
                text = gfx::fmt("Ritual at (%d,%d) succeeded", ev.x, ev.y);
                color = gfx::GREEN;
            }
            break;
        case GameEventKind::Fork:
            text = gfx::fmt("#%u (%s) laid an egg", ev.id, ev.text.c_str());
            color = gfx::BEIGE;
            break;
        case GameEventKind::Eject:
            text = gfx::fmt("#%u (%s) ejected the tile", ev.id, ev.text.c_str());
            color = gfx::Color{255, 145, 55, 255};
            break;
        case GameEventKind::Win:
            text = gfx::fmt("*** %s WINS ***", ev.text.c_str());
            color = gfx::GOLD;
            break;
        }
        _feed.push_back({_elapsed, color, std::move(text)});
    }
    _state.feedEvents.clear();

    while (_feed.size() > kFeedKeep)
        _feed.pop_front();

    // Bubbles for players that died/left must not float over nothing.
    for (auto it = _bubbles.begin(); it != _bubbles.end();)
    {
        if (it->second.until <= _elapsed || _state.players.find(it->first) == _state.players.end())
            it = _bubbles.erase(it);
        else
            ++it;
    }
}

void Interface::drawEventFeed()
{
    if (_feed.empty())
        return;

    // Newest at the bottom, anchored above the control-hint line; older lines
    // stack upward and fade out with age.
    const int baseY = _engine.screenHeight() - 92;
    const int lineH = 18;
    int drawn = 0;

    for (auto it = _feed.rbegin(); it != _feed.rend() && drawn < kFeedVisible; ++it)
    {
        const float age = _elapsed - it->t;
        if (age >= kFeedFadeEnd)
            break; // everything older is even more faded
        float alpha = 1.0f;
        if (age > kFeedFadeStart)
            alpha = 1.0f - (age - kFeedFadeStart) / (kFeedFadeEnd - kFeedFadeStart);

        const int y = baseY - drawn * lineH;
        const int w = _engine.measureText(it->text, 15);
        const auto a = [alpha](int v) { return static_cast<std::uint8_t>(v * alpha); };
        _engine.drawRect(8, y - 2, w + 8, lineH, gfx::Color{0, 0, 0, a(150)});
        _engine.drawText(it->text, 12, y, 15, gfx::Color{it->color.r, it->color.g, it->color.b, a(255)});
        ++drawn;
    }
}

void Interface::drawSpeechBubbles()
{
    if (_bubbles.empty())
        return;

    const gfx::Vec3 camFwd = gfx::normalize(gfx::sub(_camera.target, _camera.position));

    for (const auto &[id, bubble] : _bubbles)
    {
        const auto pit = _state.players.find(id);
        if (pit == _state.players.end())
            continue; // pruned next drain

        // Anchor to the smoothed display position so the bubble glides with the
        // body during a step instead of popping cell to cell.
        float fx = static_cast<float>(pit->second.getX());
        float fy = static_cast<float>(pit->second.getY());
        if (const auto ait = _playerAnimState.find(id); ait != _playerAnimState.end() && ait->second.posInit)
        {
            fx = ait->second.dispX;
            fy = ait->second.dispY;
        }
        const gfx::Vec3 head = {fx * TILE_SIZE + TILE_SIZE / 2.0f, kTileTopY + TILE_SIZE * 0.85f,
                                fy * TILE_SIZE + TILE_SIZE / 2.0f};
        if (gfx::dot(gfx::sub(head, _camera.position), camFwd) <= 0.0f)
            continue; // behind the camera; worldToScreen would mirror it

        const gfx::Vec2 sp = _engine.worldToScreen(_camera, head);

        // Last second: fade out instead of vanishing.
        const float left = bubble.until - _elapsed;
        const float alpha = left < 1.0f ? left : 1.0f;
        const auto a = [alpha](int v) { return static_cast<std::uint8_t>(v * alpha); };

        const int fontSize = 14;
        const int w = _engine.measureText(bubble.text, fontSize) + 16;
        const int h = 24;
        const int bx = static_cast<int>(sp.x) - w / 2;
        const int by = static_cast<int>(sp.y) - h - 8;

        const gfx::Color border = teamColor(pit->second.getTeam());
        _engine.drawRect(bx, by, w, h, gfx::Color{0, 0, 0, a(200)});
        _engine.drawRectLines(bx, by, w, h, gfx::Color{border.r, border.g, border.b, a(255)});
        // Small tail pointing at the head.
        _engine.drawLine(static_cast<int>(sp.x), by + h, static_cast<int>(sp.x), by + h + 6,
                         gfx::Color{border.r, border.g, border.b, a(255)});
        _engine.drawText(bubble.text, bx + 8, by + 5, fontSize, gfx::Color{255, 255, 255, a(255)});
    }
}

void Interface::drawHoverHighlight()
{
    // Faint outline on the aimed tile; the gold selection stays dominant.
    if (_hoverX < 0 || _hoverY < 0 || (_hoverX == _selectedX && _hoverY == _selectedY))
        return;

    const float wx = _hoverX * TILE_SIZE + TILE_SIZE / 2.0f;
    const float wz = _hoverY * TILE_SIZE + TILE_SIZE / 2.0f;
    const float half = (TILE_SIZE - kTileMargin) * 0.5f;
    const float y = kTileTopY + 0.30f;
    const gfx::Color c{255, 255, 255, 130};

    _engine.drawLine3D({wx - half, y, wz - half}, {wx + half, y, wz - half}, c);
    _engine.drawLine3D({wx + half, y, wz - half}, {wx + half, y, wz + half}, c);
    _engine.drawLine3D({wx + half, y, wz + half}, {wx - half, y, wz + half}, c);
    _engine.drawLine3D({wx - half, y, wz + half}, {wx - half, y, wz - half}, c);
}

void Interface::drawHoverTooltip()
{
    if (_hoverX < 0 || _hoverY < 0)
        return;

    const MapTile &tile = _map.getTile(_hoverX, _hoverY);

    int eggs = 0;
    for (const auto &[eid, egg] : _state.eggs)
    {
        (void)eid;
        if (egg.x == _hoverX && egg.y == _hoverY)
            ++eggs;
    }

    // Line 1: coordinates + occupants. Line 2: non-zero resources, initial-coded
    // (f/l/d/s/m/p/t follows MAP_RESOURCE_NAMES order); full names are one click away.
    std::string line1 = gfx::fmt("(%d,%d)", _hoverX, _hoverY);
    if (!tile.players.empty())
        line1 += gfx::fmt("  %d player%s", static_cast<int>(tile.players.size()), tile.players.size() > 1 ? "s" : "");
    if (eggs > 0)
        line1 += gfx::fmt("  %d egg%s", eggs, eggs > 1 ? "s" : "");

    std::string line2;
    for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
    {
        if (tile.resources[i] > 0)
            line2 += gfx::fmt("%c:%d  ", MAP_RESOURCE_NAMES[i][0], tile.resources[i]);
    }
    if (line2.empty() && tile.players.empty() && eggs == 0)
        line2 = "(empty)";

    const int fontSize = 14;
    const int pad = 6;
    const int lineH = 17;
    const int lines = line2.empty() ? 1 : 2;
    const int w = std::max(_engine.measureText(line1, fontSize), _engine.measureText(line2, fontSize)) + pad * 2;
    const int h = pad * 2 + lines * lineH;
    const int px = _engine.screenWidth() / 2 + 16;
    const int py = _engine.screenHeight() / 2 + 16;

    _engine.drawRect(px, py, w, h, gfx::Color{0, 0, 0, 170});
    _engine.drawRectLines(px, py, w, h, gfx::Color{255, 255, 255, 90});
    _engine.drawText(line1, px + pad, py + pad, fontSize, gfx::RAYWHITE);
    if (!line2.empty())
        _engine.drawText(line2, px + pad, py + pad + lineH, fontSize, gfx::LIGHTGRAY);
}

// ---------------------------------------------------------------------------
// HUD / stats / help
// ---------------------------------------------------------------------------
void Interface::drawHud()
{
    // Compact always-on status, top-left.
    _engine.drawText(gfx::fmt("Map %dx%d   Teams %d   Players %d   Eggs %d", _map.getWidth(), _map.getHeight(),
                              static_cast<int>(_state.teams.size()), static_cast<int>(_state.players.size()),
                              static_cast<int>(_state.eggs.size())),
                     10, 10, 18, gfx::RAYWHITE);

    const int mm = static_cast<int>(_elapsed) / 60;
    const int ss = static_cast<int>(_elapsed) % 60;
    _engine.drawText(gfx::fmt("Speed (time unit): %d  [+/- to change]   Elapsed %02d:%02d", _state.frequency, mm, ss),
                     10, 32, 16, gfx::SKYBLUE);

    if (_followedPlayer >= 0)
        _engine.drawText(gfx::fmt("Following player #%lld  (F to release)", static_cast<long long>(_followedPlayer)),
                         10, 52, 16, gfx::GOLD);

    // One-line control reminder; the full list is on H / F1.
    _engine.drawText("ZQSD: fly   Mouse: look   Space/Ctrl: up/down   Wheel: speed   LMB: select   "
                     "R: reset   F: follow   P: pause   Tab: stats   H: help   (gamepad supported)",
                     10, _engine.screenHeight() - 24, 15, gfx::LIGHTGRAY);

    if (_net && _net->closed())
        _engine.drawText("DISCONNECTED", _engine.screenWidth() / 2 - 70, 10, 20, gfx::RED);

    // Crosshair: marks what a left click will select.
    const int cx = _engine.screenWidth() / 2, cy = _engine.screenHeight() / 2;
    _engine.drawLine(cx - 8, cy, cx + 8, cy, gfx::Color{255, 255, 255, 160});
    _engine.drawLine(cx, cy - 8, cx, cy + 8, gfx::Color{255, 255, 255, 160});
}

void Interface::drawStatsPanel()
{
    // --- Aggregate the whole environment from the live model. ---
    std::array<long, MAP_RESOURCE_COUNT> resTotals{};
    for (int y = 0; y < _map.getHeight(); ++y)
        for (int x = 0; x < _map.getWidth(); ++x)
        {
            const MapTile &t = _map.getTile(x, y);
            for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
                resTotals[i] += t.resources[i];
        }

    std::array<int, 8> levelCounts{}; // levels 1..8
    for (const auto &[id, p] : _state.players)
    {
        (void)id;
        int lvl = p.getLevel();
        if (lvl >= 1 && lvl <= 8)
            levelCounts[static_cast<size_t>(lvl - 1)]++;
    }

    // Build the lines first so the panel can size itself.
    std::vector<std::pair<std::string, gfx::Color>> lines;
    lines.push_back({"GLOBAL STATS  (Tab)", gfx::GOLD});
    lines.push_back({"Resources on map:", gfx::SKYBLUE});
    long grand = 0;
    for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
    {
        grand += resTotals[i];
        lines.push_back(
            {gfx::fmt("  %-9s %ld", std::string(MAP_RESOURCE_NAMES[i]).c_str(), resTotals[i]), gfx::RAYWHITE});
    }
    lines.push_back({gfx::fmt("  %-9s %ld", "TOTAL", grand), gfx::LIGHTGRAY});

    lines.push_back({"Players per team:", gfx::SKYBLUE});
    for (const auto &team : _state.teams)
    {
        int alive = 0, top = 0;
        for (const auto &[id, p] : _state.players)
        {
            (void)id;
            if (p.getTeam() == team)
            {
                ++alive;
                if (p.getLevel() > top)
                    top = p.getLevel();
            }
        }
        lines.push_back({gfx::fmt("  %-10s %d  (max lvl %d)", team.c_str(), alive, top), teamColor(team)});
    }

    lines.push_back({"Players per level:", gfx::SKYBLUE});
    std::string lvlLine = "  ";
    for (int l = 0; l < 8; ++l)
        lvlLine += gfx::fmt("L%d:%d  ", l + 1, levelCounts[static_cast<size_t>(l)]);
    lines.push_back({lvlLine, gfx::RAYWHITE});

    lines.push_back({gfx::fmt("Eggs: %d        Incantations: %d", static_cast<int>(_state.eggs.size()),
                              static_cast<int>(_state.incanting.size())),
                     gfx::BEIGE});
    lines.push_back({gfx::fmt("Time unit: %d   Elapsed: %02d:%02d", _state.frequency, static_cast<int>(_elapsed) / 60,
                              static_cast<int>(_elapsed) % 60),
                     gfx::LIGHTGRAY});

    // Panel geometry: left side, under the HUD.
    const int pad = 18;
    const int lineH = 24;
    const int width = 530;
    const int height = pad * 2 + static_cast<int>(lines.size()) * lineH;
    const int px = 10;
    const int py = 80;

    _engine.drawRect(px, py, width, height, gfx::Color{0, 0, 0, 255});
    _engine.drawRectLines(px, py, width, height, gfx::GOLD);

    int ty = py + pad;
    for (const auto &[text, color] : lines)
    {
        _engine.drawText(text, px + pad, ty, 18, color);
        ty += lineH;
    }
}

gfx::Color Interface::teamColor(const std::string &team) const
{
    const auto it = std::find(_state.teams.begin(), _state.teams.end(), team);
    const std::size_t idx =
        (it != _state.teams.end()) ? static_cast<std::size_t>(std::distance(_state.teams.begin(), it)) : 0;
    return kTeamPalette[idx % kTeamPalette.size()];
}

void Interface::drawEndScreen()
{
    struct TeamStats
    {
        int alive{0};
        int totalLevel{0};
        int maxLevel{0};
        int level8{0};
    };

    std::unordered_map<std::string, TeamStats> teamStats;
    for (const auto &team : _state.teams)
        teamStats.try_emplace(team);

    std::vector<const aiPlayer *> alivePlayers;
    alivePlayers.reserve(_state.players.size());
    for (const auto &[id, player] : _state.players)
    {
        (void)id;
        alivePlayers.push_back(&player);
        TeamStats &stats = teamStats[player.getTeam()];
        ++stats.alive;
        stats.totalLevel += player.getLevel();
        stats.maxLevel = std::max(stats.maxLevel, player.getLevel());
        if (player.getLevel() >= 8)
            ++stats.level8;
    }
    std::sort(alivePlayers.begin(), alivePlayers.end(),
              [](const aiPlayer *a, const aiPlayer *b) { return a->getId() < b->getId(); });

    const gfx::Color winnerColor = teamColor(_state.winner);
    const int sw = _engine.screenWidth();
    const int sh = _engine.screenHeight();
    const int width = std::min(920, std::max(620, sw - 80));
    const int height = std::min(620, std::max(460, sh - 80));
    const int px = (sw - width) / 2;
    const int py = (sh - height) / 2;
    const int pad = 24;

    _engine.drawRect(px, py, width, height, gfx::Color{winnerColor.r, winnerColor.g, winnerColor.b, 72});
    _engine.drawRect(px, py, width, height, gfx::Color{0, 0, 0, 126});
    _engine.drawRectLines(px, py, width, height, winnerColor);

    _engine.drawText("ENTER to dismiss", px + width - 190, py + 24, 16, gfx::LIGHTGRAY);
    _engine.drawText("VICTORY", px + pad, py + 18, 34, winnerColor);
    _engine.drawText(gfx::fmt("%s wins", _state.winner.c_str()), px + pad, py + 58, 22, gfx::RAYWHITE);
    _engine.drawText(gfx::fmt("Alive players: %d   Eggs left: %d   Time unit: %d",
                              static_cast<int>(alivePlayers.size()), static_cast<int>(_state.eggs.size()),
                              _state.frequency),
                     px + pad, py + 88, 18, gfx::LIGHTGRAY);

    const int statsY = py + 124;
    const int teamNameW = 170;
    _engine.drawText("GLOBAL CLAN STATS", px + pad, statsY, 20, gfx::SKYBLUE);
    _engine.drawText("Team", px + pad, statsY + 34, 16, gfx::LIGHTGRAY);
    _engine.drawText("Alive", px + pad + teamNameW, statsY + 34, 16, gfx::LIGHTGRAY);
    _engine.drawText("Max", px + pad + teamNameW + 80, statsY + 34, 16, gfx::LIGHTGRAY);
    _engine.drawText("Avg", px + pad + teamNameW + 145, statsY + 34, 16, gfx::LIGHTGRAY);
    _engine.drawText("Lvl 8", px + pad + teamNameW + 215, statsY + 34, 16, gfx::LIGHTGRAY);

    int ty = statsY + 60;
    for (const auto &[team, stats] : teamStats)
    {
        const bool winner = team == _state.winner;
        const gfx::Color color = teamColor(team);
        const float avg =
            stats.alive > 0 ? static_cast<float>(stats.totalLevel) / static_cast<float>(stats.alive) : 0.0f;
        _engine.drawText(gfx::fmt("%s%s", winner ? "> " : "  ", team.c_str()), px + pad, ty, 16, color);
        _engine.drawText(gfx::fmt("%d", stats.alive), px + pad + teamNameW, ty, 16, color);
        _engine.drawText(gfx::fmt("%d", stats.maxLevel), px + pad + teamNameW + 80, ty, 16, color);
        _engine.drawText(gfx::fmt("%.1f", avg), px + pad + teamNameW + 145, ty, 16, color);
        _engine.drawText(gfx::fmt("%d", stats.level8), px + pad + teamNameW + 215, ty, 16, color);
        ty += 24;
    }

    const int listX = px + pad;
    const int listY = std::max(ty + 18, py + 276);
    const int listW = width - pad * 2;
    const int listH = py + height - pad - listY;
    const int rowH = 28;
    const int contentH = static_cast<int>(alivePlayers.size()) * rowH;
    const int maxScroll = std::max(0, contentH - std::max(0, listH - 42));
    if (_endScroll > static_cast<float>(maxScroll))
        _endScroll = static_cast<float>(maxScroll);

    _engine.drawText("ALIVE PLAYERS", listX, listY, 20, gfx::SKYBLUE);
    _engine.drawText("ID", listX, listY + 30, 16, gfx::LIGHTGRAY);
    _engine.drawText("Team", listX + 80, listY + 30, 16, gfx::LIGHTGRAY);
    _engine.drawText("Lvl", listX + 290, listY + 30, 16, gfx::LIGHTGRAY);
    _engine.drawText("Pos", listX + 360, listY + 30, 16, gfx::LIGHTGRAY);
    _engine.drawText("Dir", listX + 470, listY + 30, 16, gfx::LIGHTGRAY);
    _engine.drawText("Life", listX + 540, listY + 30, 16, gfx::LIGHTGRAY);

    const int rowsY = listY + 54;
    const int rowsH = std::max(0, listH - 54);
    for (std::size_t i = 0; i < alivePlayers.size(); ++i)
    {
        const aiPlayer &player = *alivePlayers[i];
        const int rowY = rowsY + static_cast<int>(i) * rowH - static_cast<int>(_endScroll);
        if (rowY < rowsY || rowY + rowH > rowsY + rowsH)
            continue;
        const bool winner = player.getTeam() == _state.winner;
        const gfx::Color color = winner ? winnerColor : gfx::RAYWHITE;
        if (i % 2 == 0)
            _engine.drawRect(listX, rowY - 4, listW, rowH, gfx::Color{255, 255, 255, 18});
        _engine.drawText(gfx::fmt("#%u", player.getId()), listX, rowY, 16, color);
        _engine.drawText(player.getTeam(), listX + 80, rowY, 16, color);
        _engine.drawText(gfx::fmt("%d", player.getLevel()), listX + 290, rowY, 16, color);
        _engine.drawText(gfx::fmt("%d,%d", player.getX(), player.getY()), listX + 360, rowY, 16, color);
        _engine.drawText(orientationLabel(player.getOrientation()), listX + 470, rowY, 16, color);
        _engine.drawText(gfx::fmt("%d", player.getLifeUnits()), listX + 540, rowY, 16, color);
    }

    if (maxScroll > 0)
    {
        const int barX = listX + listW - 8;
        const int barY = rowsY;
        const int barH = rowsH;
        const int knobH = std::max(28, barH * barH / std::max(barH, contentH));
        const int knobY = barY + static_cast<int>((barH - knobH) * (_endScroll / static_cast<float>(maxScroll)));
        _engine.drawRect(barX, barY, 4, barH, gfx::Color{255, 255, 255, 50});
        _engine.drawRect(barX - 2, knobY, 8, knobH, winnerColor);
    }
}

void Interface::drawHelpOverlay()
{
    static const std::array<const char *, 27> kHelp = {
        "CONTROLS  -  FREE CAMERA",
        "Mouse           look around freely",
        "ZQSD / Arrows   fly (Shift = faster)",
        "Space / Ctrl    move up / down",
        "Mouse wheel     fly speed",
        "Left click      select the aimed tile (crosshair)",
        "Double click    focus camera on that tile",
        "R               reset to the overview",
        "F               follow / unfollow selected player",
        "+ / -           simulation speed (sst)",
        "P               pause / resume",
        "PageDown / Up   step back / forward in time (1s)",
        "End             jump back to live",
        "Tab             toggle global stats",
        "M               toggle music",
        "Enter           dismiss / show end screen (after a win)",
        "H / F1          this help",
        "",
        "GAMEPAD (Xbox layout)",
        "Sticks          left: fly   right: look (L3 = faster)",
        "Triggers        RT: up   LT: down",
        "LB / RB         fly speed down / up",
        "A / X / Y / B   select / reset / follow / end screen",
        "D-pad U/D       simulation speed",
        "D-pad L/R       step back / forward in time",
        "Start / Select  pause / stats",
        "R3              this help",
    };

    const int pad = 16;
    const int lineH = 22;
    const int width = 460;
    const int height = pad * 2 + static_cast<int>(kHelp.size()) * lineH;
    const int px = _engine.screenWidth() / 2 - width / 2;
    const int py = _engine.screenHeight() / 2 - height / 2;

    _engine.drawRect(px, py, width, height, gfx::Color{0, 0, 0, 255});
    _engine.drawRectLines(px, py, width, height, gfx::GOLD);

    int ty = py + pad;
    for (size_t i = 0; i < kHelp.size(); ++i)
    {
        const bool header = (i == 0 || i == 18); // section titles
        _engine.drawText(kHelp[i], px + pad, ty, header ? 20 : 16, header ? gfx::GOLD : gfx::RAYWHITE);
        ty += lineH;
    }
}

void Interface::drawTimeline()
{
    const int sw = _engine.screenWidth();
    const float last = latestTime();
    const float frac = last > 0.0f ? (_playT / last) : 1.0f;

    const int barW = sw / 2;
    const int barH = 8;
    const int x0 = (sw - barW) / 2;
    const int y0 = _engine.screenHeight() - 52;

    // Mode label above the bar.
    if (_live)
    {
        _engine.drawText("LIVE", x0, y0 - 20, 16, gfx::GREEN);
    }
    else
    {
        _engine.drawText(gfx::fmt("PAUSED  %.1fs / %.1fs  (PageUp/Down scrub, End: live)", _playT, last), x0, y0 - 20,
                         16, gfx::GOLD);
    }

    // Track + filled portion + cursor knob.
    _engine.drawRect(x0, y0, barW, barH, gfx::Color{0, 0, 0, 160});
    _engine.drawRect(x0, y0, static_cast<int>(barW * frac), barH,
                     _live ? gfx::Color{60, 200, 90, 200} : gfx::Color{230, 190, 40, 220});
    _engine.drawRectLines(x0, y0, barW, barH, gfx::Color{255, 255, 255, 90});
    const int knobX = x0 + static_cast<int>(barW * frac);
    _engine.drawCircle(knobX, y0 + barH / 2, 5.0f, _live ? gfx::GREEN : gfx::GOLD);
}
