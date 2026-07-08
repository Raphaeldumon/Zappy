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
    _engine.loadInstancingShader("assets/shaders/lighting_instancing.vs", "assets/shaders/lighting.fs");
    _engine.enableBloom("assets/shaders/bloom_extract.fs", "assets/shaders/bloom_combine.fs");
    // Global UI font: every drawText/measureText call goes through it once
    // loaded; on failure the engine silently keeps raylib's built-in font.
    if (!_engine.loadUiFont("assets/toxigenesis bd.otf"))
        gfx::logWarn("loadUiFont: 'assets/toxigenesis bd.otf' failed to load (using built-in font)");
    loadTileTextures();
    loadResourceModels();
    loadPlayerModel();
    loadMeteoriteModel();
    loadIncantationModel();
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
    unloadIncantationModel();
    unloadMeteoriteModel();
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

constexpr float kPi = 3.14159265358979f;

// Meteorite event visuals.
constexpr const char *kMeteoriteModelPath = "assets/meteorite.glb";
constexpr const char *kIncantationModelPath = "assets/incantation.glb";
constexpr float kMeteorFallSeconds = 1.2f;              // sky -> ground
constexpr float kMeteorGlowSeconds = 1.6f;              // impact shockwave fade
constexpr float kMeteorFallHeight = TILE_SIZE * 10.0f;  // spawn altitude
constexpr float kMeteorSize = TILE_SIZE * 0.55f;        // display box edge
constexpr float kIncantationSize = TILE_SIZE * 1.35f;   // display box edge
constexpr float kIncantationSpin = 140.0f;              // degrees per second
// Torus proportions. 1.0 keeps surface distances equal to the flat grid;
// shrinking the minor scale flattens the tube (tiles get smaller around it)
// while a larger major scale widens the ring, for a broad thin donut.
constexpr float kTorusMinorScale = 0.55f;
constexpr float kTorusMajorScale = 1.30f;
constexpr float kTileHeight = 2.0f;
constexpr float kTileMargin = 0.0f;
constexpr float kTileTopY = kTileHeight / 2.0f; // surface items stand on

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

void Interface::loadMeteoriteModel()
{
    if (!fs::exists(kMeteoriteModelPath))
    {
        gfx::logWarn("loadMeteoriteModel: missing asset '%s' (falling back to sphere)", kMeteoriteModelPath);
        return;
    }
    _meteoriteModel.handle = _engine.loadModel(kMeteoriteModelPath);
    if (_meteoriteModel.handle == gfx::NoHandle)
    {
        gfx::logWarn("loadMeteoriteModel: failed to load '%s' (falling back to sphere)", kMeteoriteModelPath);
        return;
    }
    _meteoriteModel.loaded = true;

    // Same re-centre + fit as the resource models: footprint centred on the
    // origin, base at y=0, uniform scale into a kMeteorSize box.
    const gfx::BBox box = _engine.modelBounds(_meteoriteModel.handle);
    const float dx = std::max(box.max.x - box.min.x, 0.0001f);
    const float dy = std::max(box.max.y - box.min.y, 0.0001f);
    const float dz = std::max(box.max.z - box.min.z, 0.0001f);
    const gfx::Vec3 centre = {(box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f};
    _engine.translateModel(_meteoriteModel.handle, {-centre.x, -centre.y, -centre.z});
    const float s = kMeteorSize / std::max(dx, std::max(dy, dz));
    _meteoriteModel.scale = {s, s, s};
    gfx::logInfo("METEORITE '%s' bbox dx=%.2f dy=%.2f dz=%.2f scale=%.2f", kMeteoriteModelPath, dx, dy, dz, s);
}

void Interface::unloadMeteoriteModel()
{
    if (_meteoriteModel.loaded)
        _engine.unloadModel(_meteoriteModel.handle);
    _meteoriteModel = {};
}

void Interface::loadIncantationModel()
{
    if (!fs::exists(kIncantationModelPath))
    {
        gfx::logWarn("loadIncantationModel: missing asset '%s' (rings only)", kIncantationModelPath);
        return;
    }
    _incantationModel.handle = _engine.loadModel(kIncantationModelPath);
    if (_incantationModel.handle == gfx::NoHandle)
    {
        gfx::logWarn("loadIncantationModel: failed to load '%s' (rings only)", kIncantationModelPath);
        return;
    }
    _incantationModel.loaded = true;

    const gfx::BBox box = _engine.modelBounds(_incantationModel.handle);
    const float dx = std::max(box.max.x - box.min.x, 0.0001f);
    const float dy = std::max(box.max.y - box.min.y, 0.0001f);
    const float dz = std::max(box.max.z - box.min.z, 0.0001f);
    const gfx::Vec3 centre = {(box.min.x + box.max.x) * 0.5f, box.min.y, (box.min.z + box.max.z) * 0.5f};
    _engine.translateModel(_incantationModel.handle, {-centre.x, -centre.y, -centre.z});
    const float s = kIncantationSize / std::max(dx, std::max(dy, dz));
    _incantationModel.scale = {s, s, s};
    gfx::logInfo("INCANTATION '%s' bbox dx=%.2f dy=%.2f dz=%.2f scale=%.2f", kIncantationModelPath, dx, dy, dz, s);
}

void Interface::unloadIncantationModel()
{
    if (_incantationModel.loaded)
        _engine.unloadModel(_incantationModel.handle);
    _incantationModel = {};
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

bool Interface::playerPose(std::uint32_t id, int &clipIdx, float &frame) const
{
    if (_playerAnimCount <= 0)
        return false;
    const auto it = _playerAnimState.find(id);
    if (it == _playerAnimState.end())
        return false;
    const PlayerAnimState &st = it->second;
    const bool oneShot = st.oneShot != PlayerClip::Count;
    const PlayerClip clip = oneShot ? st.oneShot : st.loopClip;
    clipIdx = _clipIndex[static_cast<std::size_t>(clip)];
    frame = oneShot ? st.oneShotFrame : st.loopFrame;
    return clipIdx >= 0;
}

void Interface::loadTileTextures()
{
    _darkTileTexture = _engine.loadTexture("assets/sol_dark.png");
    if (_darkTileTexture == gfx::NoHandle)
        gfx::logWarn("loadTileTextures: failed to load assets/sol_dark.png");

    _orangeTileTexture = _engine.loadTexture("assets/sol_blanc.png");
    if (_orangeTileTexture == gfx::NoHandle)
        gfx::logWarn("loadTileTextures: failed to load assets/sol_blanc.png");

    _grassTileTexture = _engine.loadTexture("assets/grass.png");
    if (_grassTileTexture == gfx::NoHandle)
        gfx::logWarn("loadTileTextures: failed to load assets/grass.png");

    _blackGrassTileTexture = _engine.loadTexture("assets/blackgrass.png");
    if (_blackGrassTileTexture == gfx::NoHandle)
        gfx::logWarn("loadTileTextures: failed to load assets/blackgrass.png");
}

void Interface::unloadTileTextures()
{
    if (_darkTileTexture != gfx::NoHandle)
        _engine.unloadTexture(_darkTileTexture);
    if (_orangeTileTexture != gfx::NoHandle)
        _engine.unloadTexture(_orangeTileTexture);
    if (_grassTileTexture != gfx::NoHandle)
        _engine.unloadTexture(_grassTileTexture);
    if (_blackGrassTileTexture != gfx::NoHandle)
        _engine.unloadTexture(_blackGrassTileTexture);
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

    if (_torusView)
    {
        // Overview of the whole donut: above and south of its centre.
        const TorusGeom g = torusGeom();
        const float reach = g.R + g.r;
        _camera.position = {g.c.x, g.c.y + reach * 1.3f, g.c.z + reach * 1.7f};
        _flySpeed = reach * 0.8f;
        lookAt(g.c);
        updateCameraTarget();
        return;
    }

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

// ---------------------------------------------------------------------------
// Torus view
// ---------------------------------------------------------------------------
Interface::TorusGeom Interface::torusGeom() const
{
    // Radii start from the "honest" values (one trip around the major circle
    // spans the map width, around the tube the map height), then get reshaped
    // by the scale constants into a wider, flatter donut with smaller tiles.
    // On tall/square maps the major radius is pushed out so the tube never
    // self-intersects (slight stretch along x instead of a pinched hole).
    TorusGeom g{};
    const float w = _map.getWidth() * TILE_SIZE;
    const float h = _map.getHeight() * TILE_SIZE;
    g.r = h / (2.0f * kPi) * kTorusMinorScale;
    g.R = std::max(w / (2.0f * kPi) * kTorusMajorScale, g.r * 2.2f);
    g.c = {w / 2.0f, 0.0f, h / 2.0f};
    return g;
}

Interface::Surface Interface::surfaceAt(float wx, float wz, float h) const
{
    if (!_torusView)
        return {{wx, kTileTopY + h, wz}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};

    const TorusGeom g = torusGeom();
    const float u = wx / (_map.getWidth() * TILE_SIZE) * 2.0f * kPi;  // around the hole (map x)
    const float v = wz / (_map.getHeight() * TILE_SIZE) * 2.0f * kPi; // around the tube (map y)
    const float cu = std::cos(u), su = std::sin(u);
    const float cv = std::cos(v), sv = std::sin(v);

    const gfx::Vec3 n = {cv * cu, sv, cv * su};    // outward normal
    const gfx::Vec3 tu = {-su, 0.0f, cu};          // east tangent
    const gfx::Vec3 tv = {-sv * cu, cv, -sv * su}; // south tangent
    gfx::Vec3 p = {g.c.x + (g.R + g.r * cv) * cu, g.c.y + g.r * sv, g.c.z + (g.R + g.r * cv) * su};
    p = gfx::add(p, gfx::scale(n, kTileTopY + h));
    return {p, n, tu, tv};
}

void Interface::toggleTorusView()
{
    _torusView = !_torusView;
    _followedPlayer = -1; // follow is grid-only; its anchor loses meaning here
    initCamera();
    gfx::logInfo("toggleTorusView: %s", _torusView ? "TORUS" : "GRID");
}

// Sub-quads per tile per axis: small maps curve a lot per tile, so subdivide
// more; on big maps one quad per tile already hugs the surface.
static int torusSubdiv(int tiles)
{
    const int s = 24 / tiles + 1;
    return s < 1 ? 1 : (s > 6 ? 6 : s);
}

void Interface::drawTorusFloor()
{
    // Checkerboard painted straight onto the torus. Textured immediate quads
    // skip the lighting shader, so a cheap analytic lambert term stands in for it.
    const int mapW = _map.getWidth(), mapH = _map.getHeight();
    const int subU = torusSubdiv(mapW);
    const int subV = torusSubdiv(mapH);
    const gfx::Vec3 lightDir = gfx::normalize({0.35f, 0.85f, 0.30f});
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
                    const float lum = 0.45f + 0.55f * std::max(0.0f, gfx::dot(mid.up, lightDir));
                    const gfx::Color c{static_cast<std::uint8_t>(base.r * lum),
                                       static_cast<std::uint8_t>(base.g * lum),
                                       static_cast<std::uint8_t>(base.b * lum), 255};

                    const gfx::Vec3 a = surfaceAt(u0, v0, 0.0f).pos;
                    const gfx::Vec3 b = surfaceAt(u1, v0, 0.0f).pos;
                    const gfx::Vec3 cpos = surfaceAt(u1, v1, 0.0f).pos;
                    const gfx::Vec3 d = surfaceAt(u0, v1, 0.0f).pos;
                    if (tex != gfx::NoHandle)
                        _engine.drawTexturedQuad3D(tex, a, b, cpos, d, mid.up,
                                                   gfx::Color{static_cast<std::uint8_t>(255.0f * lum),
                                                              static_cast<std::uint8_t>(255.0f * lum),
                                                              static_cast<std::uint8_t>(255.0f * lum), 255});
                    else
                        _engine.drawQuad3D(a, b, cpos, d, c);
                }
            }
        }
    }
}

void Interface::drawTileEdges(int x, int y, float h, gfx::Color c)
{
    // Border of tile (x,y) slightly above the surface; subdivided in torus
    // mode so the lines follow the curvature (1 segment/edge on the flat grid).
    const int seg = _torusView ? std::max(torusSubdiv(_map.getWidth()), torusSubdiv(_map.getHeight())) + 1 : 1;
    const float x0 = x * TILE_SIZE, z0 = y * TILE_SIZE;
    auto at = [&](float fx, float fz) { return surfaceAt(x0 + fx * TILE_SIZE, z0 + fz * TILE_SIZE, h).pos; };

    for (int i = 0; i < seg; ++i)
    {
        const float a = static_cast<float>(i) / seg;
        const float b = static_cast<float>(i + 1) / seg;
        _engine.drawLine3D(at(a, 0.0f), at(b, 0.0f), c);
        _engine.drawLine3D(at(a, 1.0f), at(b, 1.0f), c);
        _engine.drawLine3D(at(0.0f, a), at(0.0f, b), c);
        _engine.drawLine3D(at(1.0f, a), at(1.0f, b), c);
    }
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

    // ---- T: swap the world between the flat grid and its true torus shape.
    if (_engine.keyPressed(gfx::Key::T))
        toggleTorusView();
    if (_engine.keyPressed(gfx::Key::V))
        _weatherVisible = !_weatherVisible;

    // ---- F: toggle riding along with the selected player (grid view only:
    // the follow anchor is a flat-world position).
    if (!_torusView && (_engine.keyPressed(gfx::Key::F) || _engine.padPressed(gfx::PadBtn::FaceUp)))
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
    if (_engine.keyPressed(gfx::Key::F3))
        _showPerf = !_showPerf;
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
    const Surface s = surfaceAt(tx * TILE_SIZE + TILE_SIZE / 2.0f, ty * TILE_SIZE + TILE_SIZE / 2.0f, 0.0f);
    if (_torusView)
    {
        // "Above" follows the local normal; back off along the local south.
        _camera.position =
            gfx::add(s.pos, gfx::add(gfx::scale(s.up, TILE_SIZE * 4.0f), gfx::scale(s.back, TILE_SIZE * 3.0f)));
    }
    else
    {
        _camera.position = {s.pos.x, TILE_SIZE * 5.0f, s.pos.z + TILE_SIZE * 5.0f};
    }
    lookAt(s.pos);
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
    updateRandomEvents(_engine.frameTime());

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

void Interface::updateRandomEvents(float dt)
{
    // Advance active meteorites; drop them once the impact glow has faded.
    for (auto &m : _meteorites)
        m.age += dt;
    _meteorites.erase(std::remove_if(_meteorites.begin(), _meteorites.end(),
                                     [](const Meteorite &m) {
                                         return m.age >= kMeteorFallSeconds + kMeteorGlowSeconds;
                                     }),
                      _meteorites.end());
}

namespace
{
constexpr float kPlayerBaseSize = TILE_SIZE * 0.4f;
constexpr float kPlayerY = 4.0f;
constexpr float kPlayerHeight = 6.0f;
constexpr float kItemSpacing = TILE_SIZE * 0.22f; // grid pitch between stacked items

// Beyond this distance resource meshes are sub-pixel: draw a small tinted
// cube (rlgl-batched) instead of a full model instance.
constexpr float kItemLodDist = TILE_SIZE * 26.0f;
constexpr std::array<gfx::Color, MAP_RESOURCE_COUNT> kResourceLodColors{{
    {235, 150, 60, 255},  // 0 food      (roast chicken)
    {200, 200, 205, 255}, // 1 linemate  (white can)
    {120, 120, 130, 255}, // 2 deraumere (black can)
    {255, 205, 60, 255},  // 3 sibur     (golden can)
    {90, 210, 120, 255},  // 4 mendiane  (green can)
    {160, 160, 170, 255}, // 5 phiras    (zero ultra can)
    {235, 120, 200, 255}, // 6 thystame  (pink can)
}};
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

const char *weatherLabel(const std::string &weather)
{
    if (weather == "rain")
        return "Rain";
    if (weather == "storm")
        return "Storm";
    if (weather == "fog")
        return "Fog";
    if (weather == "heat")
        return "Heat";
    if (weather == "fertile")
        return "Fertile";
    return "Clear";
}

const char *seasonLabel(const std::string &season)
{
    if (season == "summer")
        return "Summer";
    if (season == "autumn")
        return "Autumn";
    if (season == "winter")
        return "Winter";
    return "Spring";
}

gfx::Color seasonColor(const std::string &season)
{
    if (season == "summer")
        return gfx::Color{255, 178, 75, 255};
    if (season == "autumn")
        return gfx::Color{214, 123, 62, 255};
    if (season == "winter")
        return gfx::Color{165, 210, 235, 255};
    return gfx::Color{115, 235, 140, 255};
}
} // namespace

void Interface::render()
{
    _stats = {};
    std::vector<CountLabel> labels;

    // Player/ghost draws are collected first, then sorted so one pose upload
    // (CPU skinning) serves every model sharing the same clip + frame bucket.
    struct PlayerDrawCmd
    {
        int clip;   // -1 = no animation data, draw with whatever pose is current
        int bucket; // frame quantised to half-frames; upload key with clip
        Surface s;
        float yaw;
        gfx::Color tint;
    };
    std::vector<PlayerDrawCmd> playerDraws;

    // Per-resource-type placements, flushed as one instanced call per type.
    std::vector<std::vector<gfx::InstanceXform>> itemXf(_resourceModels.size());

    _engine.beginMode3D(_camera);

    // 360 background first, so the rest of the scene draws in front of it.
    _engine.drawSkybox();

    // Floor: two batched draws (dark + orange) instead of one per tile.
    // In torus view the checkerboard is painted on the torus surface instead.
    const bool floorTextured = _darkTileTexture != gfx::NoHandle || _orangeTileTexture != gfx::NoHandle;
    if (_torusView)
    {
        drawTorusFloor();
    }
    else if (floorTextured)
    {
        _engine.drawCheckerFloor(_darkTileTexture, _orangeTileTexture, _map.getWidth(), _map.getHeight(), TILE_SIZE,
                                 TILE_SIZE - kTileMargin, kTileTopY);
    }

    // Frustum cull: skip the expensive per-tile content (outlines, players,
    // resource models) for tiles outside the view cone. Pure dot products —
    // no worldToScreen matrix work per tile. The cone half-angle covers the
    // screen diagonal plus slack, and a per-tile angular term widens it so
    // partly-visible edge tiles still draw. The batched floor above stays
    // fully drawn — it's cheap and avoids holes at the edges.
    const gfx::Vec3 camFwd = gfx::normalize(gfx::sub(_camera.target, _camera.position));
    const float aspect = static_cast<float>(_engine.screenWidth()) / static_cast<float>(_engine.screenHeight());
    const float tanHalfV = std::tan(_camera.fovy * 0.5f * kPi / 180.0f);
    const float halfDiag = std::atan(tanHalfV * std::sqrt(1.0f + aspect * aspect));
    const float cosCone = std::cos(std::min(halfDiag + 0.10f, 1.5f));
    auto tileVisible = [&](gfx::Vec3 c) {
        const gfx::Vec3 d = gfx::sub(c, _camera.position);
        const float dist = gfx::length(d);
        if (dist < TILE_SIZE * 3.0f)
            return true; // touching the camera: always in
        return gfx::dot(d, camFwd) / dist > cosCone - TILE_SIZE * 1.6f / dist;
    };

    for (int y = 0; y < _map.getHeight(); ++y)
    {
        for (int x = 0; x < _map.getWidth(); ++x)
        {
            float worldX = x * TILE_SIZE + TILE_SIZE / 2.0f;
            float worldZ = y * TILE_SIZE + TILE_SIZE / 2.0f;
            const Surface surf = surfaceAt(worldX, worldZ, 0.0f);

            if (!tileVisible(surf.pos))
            {
                ++_stats.tilesCulled;
                continue;
            }
            ++_stats.tilesDrawn;
            const bool lodTile = gfx::length(gfx::sub(surf.pos, _camera.position)) > kItemLodDist;

            const MapTile &tile = _map.getTile(x, y);

            // Untextured fallback only (textured floor was drawn batched above).
            if (!_torusView && !floorTextured)
                _engine.drawPlane({worldX, kTileTopY, worldZ}, {TILE_SIZE - kTileMargin, TILE_SIZE - kTileMargin},
                                  gfx::WHITE);
            drawTileEdges(x, y, 0.02f, gfx::BLACK);

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
                    const Surface ps = surfaceAt(px, pz, 0.0f);
                    if (_playerModel.loaded)
                    {
                        int clipIdx = -1;
                        float frame = 0.0f;
                        const bool posed = playerPose(player.getId(), clipIdx, frame);
                        playerDraws.push_back({posed ? clipIdx : -1,
                                               posed ? static_cast<int>(frame * 2.0f) : -1, ps,
                                               playerOrientationAngle(player.getOrientation()), teamGlow});
                    }
                    else
                    {
                        _engine.drawCube(gfx::add(ps.pos, gfx::scale(ps.up, kPlayerY - kTileTopY)), kPlayerBaseSize,
                                         kPlayerHeight, kPlayerBaseSize, teamColor(player.getTeam()));
                    }
                }
                if (playerCount > kMaxVisiblePlayers)
                    labels.push_back({gfx::add(surf.pos, gfx::scale(surf.up, kPlayerY + kPlayerHeight - kTileTopY)),
                                      playerCount, gfx::YELLOW});
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

                        const Surface is = surfaceAt(cx, cz, 0.0f);
                        if (hasModel && lodTile)
                        {
                            // Too far for the mesh to read: small tinted cube.
                            _engine.drawCube(gfx::add(is.pos, gfx::scale(is.up, kItemWidth * 0.5f)), kItemWidth,
                                             kItemWidth, kItemWidth, kResourceLodColors[static_cast<std::size_t>(i)]);
                            ++_stats.itemsLod;
                        }
                        else if (hasModel)
                        {
                            itemXf[static_cast<std::size_t>(i)].push_back(
                                {is.pos, is.right, is.up, is.back, angle, _resourceModels[static_cast<std::size_t>(i)].scale});
                            ++_stats.itemsModel;
                        }
                        else
                        {
                            const float s = kItemWidth;
                            _engine.drawCube(gfx::add(is.pos, gfx::scale(is.up, s / 2.0f)), s, s, s, gfx::RED);
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
        const Surface es = surfaceAt(ex, ez, TILE_SIZE * 0.08f);
        if (!tileVisible(es.pos))
            continue;
        _engine.drawSphere(es.pos, TILE_SIZE * 0.08f, gfx::BEIGE);
        ++_stats.eggs;
    }

    // Death ghosts: players already erased from the world, replaying the Death
    // clip once at the spot they fell. Collected with the live players so they
    // share the same pose buckets.
    if (_playerModel.loaded && !_deathGhosts.empty())
    {
        const int didx = _clipIndex[static_cast<std::size_t>(PlayerClip::Death)];
        if (didx >= 0)
        {
            for (const auto &g : _deathGhosts)
            {
                const Surface gs = surfaceAt(g.x, g.y, 0.0f);
                if (!tileVisible(gs.pos))
                    continue;
                playerDraws.push_back({didx, static_cast<int>(g.frame * 2.0f), gs,
                                       playerOrientationAngle(g.orientation), gfx::WHITE});
            }
        }
    }

    // Flush players: sort by (clip, half-frame bucket) so one CPU-skinning
    // upload poses every model in the bucket. Unposed entries (clip -1) sort
    // first and draw with whatever pose the mesh currently holds.
    _stats.players = static_cast<int>(playerDraws.size());
    if (_playerModel.loaded && !playerDraws.empty())
    {
        std::sort(playerDraws.begin(), playerDraws.end(), [](const PlayerDrawCmd &a, const PlayerDrawCmd &b) {
            return a.clip != b.clip ? a.clip < b.clip : a.bucket < b.bucket;
        });
        int lastClip = -2, lastBucket = -1;
        for (const auto &pd : playerDraws)
        {
            if (pd.clip >= 0 && (pd.clip != lastClip || pd.bucket != lastBucket))
            {
                _engine.applyPose(_playerModel.handle, _playerAnims, pd.clip, static_cast<float>(pd.bucket) * 0.5f);
                ++_stats.poseUploads;
                lastClip = pd.clip;
                lastBucket = pd.bucket;
            }
            _engine.drawModelOriented(_playerModel.handle, pd.s.pos, pd.s.right, pd.s.up, pd.s.back, pd.yaw,
                                      _playerModel.scale, pd.tint);
        }
    }

    // Flush resources: one instanced call per resource type.
    for (std::size_t i = 0; i < itemXf.size(); ++i)
        if (!itemXf[i].empty())
            _engine.drawModelInstanced(_resourceModels[i].handle, itemXf[i], gfx::WHITE);

    drawMeteorites();
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

    if (_weatherVisible)
        drawWeatherOverlay();
    drawSpeechBubbles();
    drawHud();
    drawTimeline();
    drawEventFeed();
    drawTileInfoPanel();
    drawHoverTooltip();
    if (_showPerf)
        drawPerfOverlay();
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

    if (_torusView)
    {
        // Sphere-trace the torus SDF, then invert the (u, v) angles at the hit
        // point back into tile coordinates. Cheap and exact enough for picking.
        const TorusGeom g = torusGeom();
        const auto sdf = [&](gfx::Vec3 p) {
            const float dx = p.x - g.c.x, dy = p.y - g.c.y, dz = p.z - g.c.z;
            const float q = std::sqrt(dx * dx + dz * dz) - g.R;
            return std::sqrt(q * q + dy * dy) - g.r;
        };
        const float maxT = (g.R + g.r) * 6.0f;
        float t = 0.0f;
        for (int i = 0; i < 128 && t < maxT; ++i)
        {
            const gfx::Vec3 p = gfx::add(ray.position, gfx::scale(ray.direction, t));
            const float d = sdf(p);
            if (d < 0.1f)
            {
                const float dx = p.x - g.c.x, dy = p.y - g.c.y, dz = p.z - g.c.z;
                const float q = std::sqrt(dx * dx + dz * dz) - g.R;
                const float twoPi = 2.0f * kPi;
                float u = std::atan2(dz, dx);
                float v = std::atan2(dy, q);
                u = u < 0.0f ? u + twoPi : u;
                v = v < 0.0f ? v + twoPi : v;
                tx = std::min(_map.getWidth() - 1, static_cast<int>(u / twoPi * _map.getWidth()));
                ty = std::min(_map.getHeight() - 1, static_cast<int>(v / twoPi * _map.getHeight()));
                return true;
            }
            t += d;
        }
        return false; // aiming past the donut
    }

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

void Interface::drawMeteorites()
{
    for (const auto &m : _meteorites)
    {
        const Surface s = surfaceAt(m.x * TILE_SIZE + TILE_SIZE / 2.0f, m.y * TILE_SIZE + TILE_SIZE / 2.0f, 0.0f);
        // Spin while falling, frozen once embedded (continuity at impact).
        const float spin = std::min(m.age, kMeteorFallSeconds) * 540.0f;

        if (m.age < kMeteorFallSeconds)
        {
            // Falling: quadratic ease-in down the local normal (reads as gravity).
            const float p = m.age / kMeteorFallSeconds;
            const float h = kMeteorFallHeight * (1.0f - p * p);
            const gfx::Vec3 pos = gfx::add(s.pos, gfx::scale(s.up, h));

            if (_meteoriteModel.loaded)
                _engine.drawModelOriented(_meteoriteModel.handle, pos, s.right, s.up, s.back, spin,
                                          _meteoriteModel.scale, gfx::WHITE);
            else
                _engine.drawSphere(gfx::add(pos, gfx::scale(s.up, kMeteorSize * 0.5f)), kMeteorSize * 0.5f,
                                   gfx::Color{140, 95, 60, 255});

            // Short fire trail above the rock; bright enough for the bloom pass.
            for (int i = 1; i <= 3; ++i)
            {
                const float ta = 1.0f - static_cast<float>(i) * 0.28f;
                _engine.drawSphere(gfx::add(pos, gfx::scale(s.up, kMeteorSize * 0.5f + i * TILE_SIZE * 0.45f)),
                                   kMeteorSize * (0.30f - 0.06f * i),
                                   gfx::Color{255, 170, 70, static_cast<std::uint8_t>(220.0f * ta)});
            }
        }
        else
        {
            // Impact: rock embedded on the tile, expanding shockwave ring and
            // a bright fading glow quad (the bloom pass makes it flare).
            const float g = (m.age - kMeteorFallSeconds) / kMeteorGlowSeconds; // 0..1
            const auto fade = static_cast<std::uint8_t>(230.0f * (1.0f - g));

            if (_meteoriteModel.loaded)
                _engine.drawModelOriented(_meteoriteModel.handle, s.pos, s.right, s.up, s.back, spin,
                                          _meteoriteModel.scale, gfx::WHITE);
            else
                _engine.drawSphere(gfx::add(s.pos, gfx::scale(s.up, kMeteorSize * 0.3f)), kMeteorSize * 0.5f,
                                   gfx::Color{140, 95, 60, 255});

            const gfx::Vec3 ringPos = gfx::add(s.pos, gfx::scale(s.up, 0.3f));
            _engine.drawCircle3D(ringPos, TILE_SIZE * (0.2f + 0.9f * g), s.up, gfx::Color{255, 160, 60, fade});
            _engine.drawCircle3D(ringPos, TILE_SIZE * (0.1f + 0.6f * g), s.up, gfx::Color{255, 210, 120, fade});

            const float hs = TILE_SIZE * 0.35f * (1.0f - 0.4f * g);
            const gfx::Vec3 dx = gfx::scale(s.right, hs), dz = gfx::scale(s.back, hs);
            _engine.drawQuad3D(gfx::sub(gfx::sub(ringPos, dx), dz), gfx::sub(gfx::add(ringPos, dx), dz),
                               gfx::add(gfx::add(ringPos, dx), dz), gfx::add(gfx::sub(ringPos, dx), dz),
                               gfx::Color{255, 190, 90, static_cast<std::uint8_t>(fade * 0.7f)});
        }
    }
}

void Interface::drawIncantationRings()
{
    if (_state.incanting.empty())
        return;

    // Three expanding, fading rings per ritual tile.
    // The colours sit above the bloom threshold, so the whole thing glows.
    constexpr int kRings = 3;
    const gfx::Color kRitual{210, 130, 255, 255};
    const float maxR = TILE_SIZE * 0.46f;

    for (const long long key : _state.incanting)
    {
        const int tx = static_cast<int>(key % _map.getWidth());
        const int ty = static_cast<int>(key / _map.getWidth());
        const Surface s =
            surfaceAt(tx * TILE_SIZE + TILE_SIZE / 2.0f, ty * TILE_SIZE + TILE_SIZE / 2.0f, 0.25f);

        if (_incantationModel.loaded)
        {
            const float spin = std::fmod(_elapsed * kIncantationSpin, 360.0f);
            _engine.drawModelOriented(_incantationModel.handle, s.pos, s.right, s.up, s.back, spin,
                                      _incantationModel.scale, gfx::WHITE);
        }

        for (int i = 0; i < kRings; ++i)
        {
            // Phase-offset sawtooth: each ring grows 0 -> maxR then wraps.
            float phase = _elapsed * 0.7f + static_cast<float>(i) / kRings;
            phase -= std::floor(phase);
            const float r = maxR * phase;
            const auto alpha = static_cast<std::uint8_t>(230.0f * (1.0f - phase));
            _engine.drawCircle3D(s.pos, r, s.up, gfx::Color{kRitual.r, kRitual.g, kRitual.b, alpha});
        }
    }
}

void Interface::drawSelectionHighlight()
{
    if (_selectedX < 0 || _selectedY < 0)
        return;

    const float wx = _selectedX * TILE_SIZE + TILE_SIZE / 2.0f;
    const float wz = _selectedY * TILE_SIZE + TILE_SIZE / 2.0f;
    const float half = (TILE_SIZE - kTileMargin) * 0.5f;

    // Translucent overlay + bright gold border just above the tile surface.
    // The fill breathes slowly so the selection reads as "active", not painted.
    const float breathe = 0.5f + 0.5f * std::sin(_elapsed * 3.0f);
    const auto fillA = static_cast<std::uint8_t>(45.0f + 45.0f * breathe);
    const Surface s = surfaceAt(wx, wz, 0.15f);
    const gfx::Vec3 dx = gfx::scale(s.right, half), dz = gfx::scale(s.back, half);
    _engine.drawQuad3D(gfx::sub(gfx::sub(s.pos, dx), dz), gfx::sub(gfx::add(s.pos, dx), dz),
                       gfx::add(gfx::add(s.pos, dx), dz), gfx::add(gfx::sub(s.pos, dx), dz),
                       gfx::Color{255, 230, 0, fillA});

    drawTileEdges(_selectedX, _selectedY, 0.35f, gfx::GOLD);
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
        case GameEventKind::Meteor:
            text = gfx::fmt("Meteorite strike at (%d, %d)!", ev.x, ev.y);
            color = gfx::Color{255, 130, 50, 255};
            _meteorites.push_back({ev.x, ev.y, 0.0f});
            gfx::logInfo("meteorite event at (%d, %d)", ev.x, ev.y);
            break;
        case GameEventKind::Weather:
        {
            const auto sep = ev.text.find(':');
            const std::string season = sep == std::string::npos ? _state.season : ev.text.substr(0, sep);
            const std::string weather = sep == std::string::npos ? ev.text : ev.text.substr(sep + 1);
            text = gfx::fmt("%s: %s", seasonLabel(season), weatherLabel(weather));
            color = seasonColor(season);
            break;
        }
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
        const gfx::Vec3 head =
            surfaceAt(fx * TILE_SIZE + TILE_SIZE / 2.0f, fy * TILE_SIZE + TILE_SIZE / 2.0f, TILE_SIZE * 0.85f).pos;
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

    drawTileEdges(_hoverX, _hoverY, 0.30f, gfx::Color{255, 255, 255, 130});
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
    _engine.drawText(gfx::fmt("Season: %s   Weather: %s   FX: %s", seasonLabel(_state.season),
                              weatherLabel(_state.weather), _weatherVisible ? "on" : "off"),
                     10, 52, 16, seasonColor(_state.season));

    if (_followedPlayer >= 0)
        _engine.drawText(gfx::fmt("Following player #%lld  (F to release)", static_cast<long long>(_followedPlayer)),
                         10, 72, 16, gfx::GOLD);

    if (_net && _net->closed())
        _engine.drawText("DISCONNECTED", _engine.screenWidth() / 2 - 70, 10, 20, gfx::RED);

    // Crosshair: marks what a left click will select.
    const int cx = _engine.screenWidth() / 2, cy = _engine.screenHeight() / 2;
    _engine.drawLine(cx - 8, cy, cx + 8, cy, gfx::Color{255, 255, 255, 160});
    _engine.drawLine(cx, cy - 8, cx, cy + 8, gfx::Color{255, 255, 255, 160});
}

void Interface::drawWeatherOverlay()
{
    const std::string &weather = _state.weather;
    const std::string &season = _state.season;

    const int sw = _engine.screenWidth();
    const int sh = _engine.screenHeight();
    const float t = _elapsed;

    if (season == "spring")
    {
        const auto alpha = static_cast<std::uint8_t>(weather == "clear" ? 18 : 28);
        _engine.drawRect(0, 0, sw, sh, gfx::Color{35, 115, 70, alpha});
        for (int i = 0; i < 34; ++i)
        {
            const float phase = t * (14.0f + (i % 5)) + static_cast<float>(i * 61);
            const int x = static_cast<int>(std::fmod(phase * 1.9f + std::sin(phase * 0.07f) * 55.0f,
                                                     static_cast<float>(sw)));
            const int y = static_cast<int>(std::fmod(phase * 0.65f + i * 91.0f, static_cast<float>(sh)));
            const gfx::Color c = i % 3 == 0 ? gfx::Color{255, 220, 130, 80} : gfx::Color{145, 255, 165, 65};
            _engine.drawCircle(x, y, 1.5f + static_cast<float>(i % 3), c);
        }
    }
    else if (season == "summer")
    {
        const auto alpha = static_cast<std::uint8_t>(weather == "heat" ? 38 : 22);
        _engine.drawRect(0, 0, sw, sh, gfx::Color{255, 170, 45, alpha});
        _engine.drawCircle(sw - 92, 88, 38.0f + std::sin(t * 1.4f) * 4.0f, gfx::Color{255, 220, 95, 90});
        _engine.drawCircle(sw - 92, 88, 58.0f + std::sin(t * 1.1f) * 5.0f, gfx::Color{255, 180, 55, 38});
    }
    else if (season == "autumn")
    {
        _engine.drawRect(0, 0, sw, sh, gfx::Color{150, 80, 35, 34});
        for (int i = 0; i < 30; ++i)
        {
            const float fall = std::fmod(t * (24.0f + i % 7) + static_cast<float>(i * 41), static_cast<float>(sh + 80));
            const int x = static_cast<int>(std::fmod(i * 97.0f + std::sin(t * 1.8f + i) * 72.0f + fall * 0.28f,
                                                     static_cast<float>(sw)));
            const int y = static_cast<int>(fall) - 40;
            const gfx::Color leaf = i % 3 == 0 ? gfx::Color{235, 105, 45, 115}
                                   : i % 3 == 1 ? gfx::Color{215, 155, 55, 110}
                                                : gfx::Color{145, 82, 42, 105};
            _engine.drawRect(x, y, 7 + i % 4, 3 + i % 3, leaf);
        }
    }
    else if (season == "winter")
    {
        const auto alpha = static_cast<std::uint8_t>(weather == "fog" ? 55 : 34);
        _engine.drawRect(0, 0, sw, sh, gfx::Color{135, 175, 205, alpha});
        for (int i = 0; i < 24; ++i)
        {
            const float fall = std::fmod(t * (18.0f + i % 5) + static_cast<float>(i * 83), static_cast<float>(sh + 30));
            const int x = static_cast<int>(std::fmod(i * 121.0f + std::sin(t + i) * 35.0f,
                                                     static_cast<float>(sw)));
            _engine.drawCircle(x, static_cast<int>(fall) - 15, 1.4f + static_cast<float>(i % 2),
                               gfx::Color{235, 245, 255, 72});
        }
    }

    if (weather == "clear")
        return;

    if (weather == "rain" || weather == "storm")
    {
        const bool storm = weather == "storm";
        _engine.drawRect(0, 0, sw, sh, storm ? gfx::Color{18, 18, 42, 102} : gfx::Color{28, 55, 85, 58});
        const int step = storm ? 24 : 34;
        const int slant = storm ? 24 : 15;
        const int len = storm ? 34 : 24;
        const int fall = static_cast<int>(std::fmod(t * (storm ? 520.0f : 360.0f), static_cast<float>(step)));
        const gfx::Color drop = storm ? gfx::Color{185, 190, 255, 190} : gfx::Color{150, 205, 255, 150};
        for (int y = -step; y < sh + step; y += step)
        {
            for (int x = -step; x < sw + step; x += step)
            {
                const int jitter = ((x * 17 + y * 31) & 15) - 8;
                const int sx = x + jitter + fall;
                const int sy = y + fall;
                _engine.drawLine(sx, sy, sx - slant, sy + len, drop);
            }
        }
        if (storm && std::fmod(t, 5.5f) < 0.16f)
            _engine.drawRect(0, 0, sw, sh, gfx::Color{230, 230, 255, 70});
        return;
    }

    if (weather == "fog")
    {
        _engine.drawRect(0, 0, sw, sh, gfx::Color{190, 200, 205, 88});
        for (int i = 0; i < 10; ++i)
        {
            const float drift = std::fmod(t * (16.0f + i * 3.0f), static_cast<float>(sw + 340));
            const int x = static_cast<int>(drift) - 340;
            const int y = 55 + i * (sh / 11);
            _engine.drawRect(x, y, 340, 22 + (i % 3) * 6, gfx::Color{225, 230, 232, 42});
        }
        return;
    }

    if (weather == "heat")
    {
        _engine.drawRect(0, 0, sw, sh, gfx::Color{255, 95, 20, 48});
        for (int y = 80; y < sh; y += 55)
        {
            const int offset = static_cast<int>(std::sin(t * 3.0f + y * 0.04f) * 18.0f);
            _engine.drawLine(0, y, sw, y + offset, gfx::Color{255, 205, 95, 50});
            _engine.drawLine(0, y + 12, sw, y + 12 - offset, gfx::Color{255, 150, 70, 35});
        }
        return;
    }

    if (weather == "fertile")
    {
        _engine.drawRect(0, 0, sw, sh, gfx::Color{25, 150, 65, 42});
        for (int i = 0; i < 44; ++i)
        {
            const float phase = t * 26.0f + static_cast<float>(i * 47);
            const int x = static_cast<int>(std::fmod(phase * 1.55f + std::cos(phase * 0.09f) * 42.0f,
                                                     static_cast<float>(sw)));
            const int y = static_cast<int>(std::fmod(phase * 0.82f + i * 83.0f, static_cast<float>(sh)));
            _engine.drawCircle(x, y, 2.0f + static_cast<float>(i % 4), gfx::Color{135, 255, 150, 92});
        }
    }
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
    static const std::array<const char *, 30> kHelp = {
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
        "T               grid <-> torus world view",
        "V               toggle weather visuals",
        "M               toggle music",
        "Enter           dismiss / show end screen (after a win)",
        "H / F1          this help",
        "F3              perf counters overlay",
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
        const bool header = (i == 0 || i == 21); // section titles
        _engine.drawText(kHelp[i], px + pad, ty, header ? 20 : 16, header ? gfx::GOLD : gfx::RAYWHITE);
        ty += lineH;
    }
}

void Interface::drawPerfOverlay()
{
    const std::array<std::string, 7> lines = {
        gfx::fmt("frame        %5.2f ms", _engine.frameTime() * 1000.0f),
        gfx::fmt("tiles        %d drawn / %d culled", _stats.tilesDrawn, _stats.tilesCulled),
        gfx::fmt("players      %d (poses %d)", _stats.players, _stats.poseUploads),
        gfx::fmt("items        %d mesh / %d lod", _stats.itemsModel, _stats.itemsLod),
        gfx::fmt("eggs         %d", _stats.eggs),
        gfx::fmt("view         %s", _torusView ? "torus" : "grid"),
        "F3 to close",
    };

    const int lineH = 20;
    const int w = 300;
    const int h = 16 + static_cast<int>(lines.size()) * lineH;
    const int x = _engine.screenWidth() - w - 10;
    const int y = 36; // under the FPS counter

    _engine.drawRect(x, y, w, h, gfx::Color{0, 0, 0, 175});
    _engine.drawRectLines(x, y, w, h, gfx::GREEN);
    int ty = y + 8;
    for (const auto &line : lines)
    {
        _engine.drawText(line, x + 10, ty, 15, gfx::RAYWHITE);
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
