#include "interface.hpp"
#include "raymath.h"
#include "rlgl.h"

#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Interface::Interface(std::unique_ptr<NetClient> net, int mapWidth, int mapHeight, int windowWidth, int windowHeight)
    : _engine(windowWidth, windowHeight, std::string(WINDOW_TITLE))
    , _map(mapWidth, mapHeight)
    , _net(std::move(net))
{
    initCamera();
    loadSkybox();         // needs the GL context, so after the engine init
    loadTileTextures();
    loadResourceModels();
    loadPlayerModel();
    loadBackgroundMusic();
    DisableCursor();      // capture the mouse for free-look (Esc releases on close)
}

Interface::~Interface()
{
    // Member destruction runs after this body, so the GL context (owned by
    // _engine) is still alive here — safe to free GPU resources.
    EnableCursor();
    unloadBackgroundMusic();
    unloadPlayerModel();
    unloadResourceModels();
    unloadTileTextures();
    unloadSkybox();
}


namespace {
    // Index-aligned with MapTile::resources / MAP_RESOURCE_COUNT.
    // std::array's size is checked at compile time against MAP_RESOURCE_COUNT below,
    // so a mismatch between the resource enum and this list fails to build instead
    // of corrupting memory at runtime.
    // One mesh per resource, index-aligned with MAP_RESOURCE_NAMES. Food has a
    // dedicated chicken mesh; the ore types use monster meshes whose embedded
    // texture identifies the mineral.
    constexpr std::array<const char*, MAP_RESOURCE_COUNT> kResourceModelPaths = {
        "assets/roast_chicken.glb",         // 0 food
        "assets/monster_black.glb",         // 1 linemate
        "assets/monster_blue.glb",          // 2 deraumere
        "assets/monster_golden.glb",        // 3 sibur
        "assets/monster_green.glb",         // 4 mendiane
        "assets/monster_zero_ultra.glb",    // 5 phiras (was monster_lightPink, dup of linemate)
        "assets/monster_pink.glb"           // 6 thystame
    };

    constexpr std::array<const char*, MAP_RESOURCE_COUNT> kResourceTextureOverrides = {
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
    constexpr float kItemWidth  = TILE_SIZE * 0.16f;
    constexpr float kItemHeight = TILE_SIZE * 0.40f;
    constexpr float kFlatWidth  = TILE_SIZE * 0.40f; // footprint for flat models (roast chicken)

    constexpr const char* kPlayerModelPath = "assets/ai_model3d.glb";
    constexpr float kPlayerModelHeight = TILE_SIZE * 0.70f;
    constexpr float kPlayerModelFootprint = TILE_SIZE * 0.46f;
    constexpr const char* kMusicPath = "assets/music_back.mp3";
    constexpr float kMusicVolume = 0.45f;

    bool isMusicTogglePressed()
    {
        if (IsKeyPressed(KEY_M))
            return true;

        int key = GetCharPressed();
        while (key > 0) {
            if (key == 'm' || key == 'M')
                return true;
            key = GetCharPressed();
        }
        return false;
    }
}


// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------
void Interface::run()
{
    while (!_engine.shouldClose()) {
        handleInput();
        update();

        _engine.beginDrawing();
        render();
        _engine.endDrawing();
    }
}

GameMap& Interface::getMap()             { return _map; }
const GameMap& Interface::getMap() const { return _map; }

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------
void Interface::loadResourceModels()
{
    _resourceModels.reserve(kResourceModelPaths.size());

    for (size_t i = 0; i < kResourceModelPaths.size(); ++i) {
        const char* path = kResourceModelPaths[i];
        ResourceModel rm;

        if (!FileExists(path)) {
            TraceLog(LOG_WARNING, "loadResourceModels: missing asset '%s' (falling back to cube)", path);
        } else {
            rm.model = LoadModel(path);
            if (rm.model.meshCount <= 0) {
                TraceLog(LOG_WARNING, "loadResourceModels: failed to load '%s' (falling back to cube)", path);
            } else {
                rm.loaded = true;

                // Some meshes embed their base colour as JPEG, which raylib does
                // not decode by default — bind a PNG override so the mesh shows up
                // textured instead of plain white.
                const char* tex = kResourceTextureOverrides[i];
                if (tex != nullptr && FileExists(tex) && rm.model.materialCount > 0) {
                    Texture2D t = LoadTexture(tex);
                    if (t.id != 0) {
                        // raylib prepends a default material at index 0 and puts the
                        // real glTF materials at index 1+, so binding only [0] misses
                        // the material the mesh actually uses. Apply to all of them.
                        for (int m = 0; m < rm.model.materialCount; ++m)
                            SetMaterialTexture(&rm.model.materials[m], MATERIAL_MAP_DIFFUSE, t);
                    } else {
                        TraceLog(LOG_WARNING, "loadResourceModels: override texture '%s' failed to load", tex);
                    }
                }

                // Re-centre the mesh: different assets have their origin in
                // arbitrary places (off to the side, above the mesh, etc.) Bake a
                // transform so the footprint is centred on the origin and the base
                // rests at y=0 — then every instance can just be dropped at a tile
                // cell + surface height and rotated in place.
                BoundingBox box = GetModelBoundingBox(rm.model);
                float dx = box.max.x - box.min.x;
                float dy = box.max.y - box.min.y;
                float dz = box.max.z - box.min.z;
                Vector3 centre = { (box.min.x + box.max.x) * 0.5f,
                                   box.min.y,
                                   (box.min.z + box.max.z) * 0.5f };
                rm.model.transform = MatrixMultiply(rm.model.transform,
                                                    MatrixTranslate(-centre.x, -centre.y, -centre.z));

                const float sx = std::max(dx, 0.0001f);
                const float sy = std::max(dy, 0.0001f);
                const float sz = std::max(dz, 0.0001f);
                const float foot = std::max(sx, sz);
                if (sy >= foot) {
                    // Upright object → force into a uniform box so every can is
                    // the same size whatever its native proportions.
                    rm.scale = { kItemWidth / sx, kItemHeight / sy, kItemWidth / sz };
                } else {
                    // Flat object (roast chicken) → uniform scale on a larger
                    // footprint so it fills the tile instead of looking tiny.
                    const float s = kFlatWidth / foot;
                    rm.scale = { s, s, s };
                }

                TraceLog(LOG_INFO, "RESMODEL %zu '%s' bbox dx=%.2f dy=%.2f dz=%.2f scale=(%.1f,%.1f,%.1f)",
                         i, path, dx, dy, dz, rm.scale.x, rm.scale.y, rm.scale.z);
            }
        }

        _resourceModels.push_back(rm);
    }
}

void Interface::unloadResourceModels()
{
    // UnloadModel also frees the material-map textures we bound above.
    for (auto& rm : _resourceModels) {
        if (rm.loaded)
            UnloadModel(rm.model);
    }
    _resourceModels.clear();
}

void Interface::loadPlayerModel()
{
    if (!FileExists(kPlayerModelPath)) {
        TraceLog(LOG_WARNING, "loadPlayerModel: missing asset '%s' (falling back to cube)", kPlayerModelPath);
        return;
    }

    _playerModel.model = LoadModel(kPlayerModelPath);
    if (_playerModel.model.meshCount <= 0) {
        TraceLog(LOG_WARNING, "loadPlayerModel: failed to load '%s' (falling back to cube)", kPlayerModelPath);
        return;
    }

    _playerModel.loaded = true;

    BoundingBox box = GetModelBoundingBox(_playerModel.model);
    const float dx = box.max.x - box.min.x;
    const float dy = box.max.y - box.min.y;
    const float dz = box.max.z - box.min.z;
    const Vector3 centre = { (box.min.x + box.max.x) * 0.5f,
                             box.min.y,
                             (box.min.z + box.max.z) * 0.5f };
    _playerModel.model.transform = MatrixMultiply(_playerModel.model.transform,
                                                  MatrixTranslate(-centre.x, -centre.y, -centre.z));

    const float sx = std::max(dx, 0.0001f);
    const float sy = std::max(dy, 0.0001f);
    const float sz = std::max(dz, 0.0001f);
    const float footprint = std::max(sx, sz);
    const float s = std::min(kPlayerModelHeight / sy, kPlayerModelFootprint / footprint);
    _playerModel.scale = { s, s, s };

    TraceLog(LOG_INFO, "PLAYERMODEL '%s' bbox dx=%.2f dy=%.2f dz=%.2f scale=%.1f",
             kPlayerModelPath, dx, dy, dz, s);
}

void Interface::unloadPlayerModel()
{
    if (_playerModel.loaded)
        UnloadModel(_playerModel.model);
    _playerModel = {};
}

void Interface::loadTileTextures()
{
    _darkTileTexture = LoadTexture("assets/sol_dark.png");
    if (_darkTileTexture.id == 0)
        TraceLog(LOG_WARNING, "loadTileTextures: failed to load assets/sol_dark.png");

    _orangeTileTexture = LoadTexture("assets/sol_orange.png");
    if (_orangeTileTexture.id == 0)
        TraceLog(LOG_WARNING, "loadTileTextures: failed to load assets/sol_orange.png");
}

void Interface::unloadTileTextures()
{
    if (_darkTileTexture.id != 0)
        UnloadTexture(_darkTileTexture);
    if (_orangeTileTexture.id != 0)
        UnloadTexture(_orangeTileTexture);
}

void Interface::loadSkybox()
{
    // Equirectangular 360 panorama (2:1). Swap this file to change the sky.
    const char* path = "assets/Background.png";
    const char* vs   = "assets/shaders/skybox.vs";
    const char* fs   = "assets/shaders/skybox.fs";
    if (!FileExists(path)) {
        TraceLog(LOG_WARNING, "loadSkybox: missing '%s' — no background", path);
        return;
    }
    if (!FileExists(vs) || !FileExists(fs)) {
        TraceLog(LOG_WARNING, "loadSkybox: missing shader files — no background");
        return;
    }
    _skyboxTex = LoadTexture(path);
    if (_skyboxTex.id == 0) {
        TraceLog(LOG_WARNING, "loadSkybox: failed to load '%s'", path);
        return;
    }
    SetTextureFilter(_skyboxTex, TEXTURE_FILTER_BILINEAR);

    _skyShader = LoadShader(vs, fs);
    if (_skyShader.id == 0) {
        TraceLog(LOG_WARNING, "loadSkybox: shader failed to compile — no background");
        UnloadTexture(_skyboxTex);
        _skyboxTex = {};
        return;
    }

    // A unit cube: the fragment shader turns each cube-local position into a
    // view ray and samples the panorama equirectangularly. raylib binds the
    // diffuse map to the shader's texture0 sampler.
    _skybox = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    _skybox.materials[0].shader = _skyShader;
    _skybox.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = _skyboxTex;
    _skyboxLoaded = true;
    TraceLog(LOG_INFO, "loadSkybox: '%s' ready", path);
}

void Interface::unloadSkybox()
{
    if (_skyboxLoaded) {
        UnloadModel(_skybox); // does not free the texture map we assigned
        UnloadShader(_skyShader);
    }
    if (_skyboxTex.id != 0)
        UnloadTexture(_skyboxTex);
    _skyboxLoaded = false;
}

void Interface::drawSkybox()
{
    if (!_skyboxLoaded)
        return;
    // Draw first, with depth test/write and backface culling off: the box is
    // centred on the eye (translation dropped in the shader) and fills the view
    // by direction, so the rest of the scene paints over it.
    rlDisableBackfaceCulling();
    rlDisableDepthMask();
    rlDisableDepthTest();
    DrawModel(_skybox, Vector3{ 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
    rlEnableDepthTest();
    rlEnableDepthMask();
    rlEnableBackfaceCulling();
}

void Interface::initCamera()
{
    // Start as a free camera hovering south of the map centre, looking down
    // onto the board — a sensible overview the user then flies away from.
    const float centerX = (_map.getWidth()  * TILE_SIZE) / 2.0f;
    const float centerZ = (_map.getHeight() * TILE_SIZE) / 2.0f;
    const float span    = std::max(_map.getWidth(), _map.getHeight()) * TILE_SIZE;

    _camera.up         = { 0.0f, 1.0f, 0.0f };
    _camera.fovy       = 60.0f; // a touch wider feels better for a free cam
    _camera.projection = CAMERA_PERSPECTIVE;

    _camera.position = { centerX, span * 0.9f, centerZ + span * 0.9f };
    _flySpeed        = span * 0.6f; // units/sec; tuned to the map scale

    // Aim at the board centre, then derive yaw/pitch from that direction.
    lookAt({ centerX, 0.0f, centerZ });
    updateCameraTarget();
}

void Interface::lookAt(Vector3 worldTarget)
{
    Vector3 dir = Vector3Subtract(worldTarget, _camera.position);
    float   horiz = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    _camYaw   = std::atan2(dir.x, dir.z);
    _camPitch = std::atan2(dir.y, horiz);
}

void Interface::updateCameraTarget()
{
    // Forward from yaw/pitch; the look target is just eye + forward.
    const float cp = std::cos(_camPitch);
    const Vector3 forward = {
        cp * std::sin(_camYaw),
        std::sin(_camPitch),
        cp * std::cos(_camYaw),
    };
    _camera.target = Vector3Add(_camera.position, forward);
}

void Interface::loadBackgroundMusic()
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        TraceLog(LOG_WARNING, "loadBackgroundMusic: audio device failed to initialize");
        return;
    }
    _audioReady = true;

    if (!FileExists(kMusicPath)) {
        TraceLog(LOG_WARNING, "loadBackgroundMusic: missing asset '%s'", kMusicPath);
        return;
    }

    _backgroundMusic = LoadMusicStream(kMusicPath);
    if (_backgroundMusic.stream.buffer == nullptr) {
        TraceLog(LOG_WARNING, "loadBackgroundMusic: failed to load '%s'", kMusicPath);
        return;
    }

    _backgroundMusic.looping = true;
    SetMusicVolume(_backgroundMusic, kMusicVolume);
    _musicLoaded = true;
    _musicEnabled = true;
    PlayMusicStream(_backgroundMusic);
    TraceLog(LOG_INFO, "loadBackgroundMusic: playing '%s'", kMusicPath);
}

void Interface::unloadBackgroundMusic()
{
    if (_musicLoaded) {
        StopMusicStream(_backgroundMusic);
        UnloadMusicStream(_backgroundMusic);
        _musicLoaded = false;
    }
    if (_audioReady) {
        CloseAudioDevice();
        _audioReady = false;
    }
}

void Interface::toggleMusic()
{
    if (!_musicLoaded)
        return;

    _musicEnabled = !_musicEnabled;
    if (_musicEnabled)
        ResumeMusicStream(_backgroundMusic);
    else
        PauseMusicStream(_backgroundMusic);
    TraceLog(LOG_INFO, "toggleMusic: music %s", _musicEnabled ? "ON" : "OFF");
}

// ---------------------------------------------------------------------------
// Loop steps
// ---------------------------------------------------------------------------
void Interface::handleInput()
{
    const float dt = GetFrameTime();

    // ---- Free look: the captured mouse turns the head (yaw/pitch).
    Vector2 md = GetMouseDelta();
    _camYaw   -= md.x * 0.0030f;
    _camPitch -= md.y * 0.0030f;
    // Clamp pitch just shy of straight up/down so the view never flips.
    const float limit = 1.553f; // ~89deg
    if (_camPitch >  limit) _camPitch =  limit;
    if (_camPitch < -limit) _camPitch = -limit;

    // ---- Movement basis from the current heading.
    const float cp = std::cos(_camPitch);
    const Vector3 forward = { cp * std::sin(_camYaw), std::sin(_camPitch), cp * std::cos(_camYaw) };
    const Vector3 rightH  = { -std::cos(_camYaw), 0.0f, std::sin(_camYaw) }; // strafe stays level

    const float boost = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 3.0f : 1.0f;
    const float step  = _flySpeed * dt * boost;
    Vector3 move{ 0.0f, 0.0f, 0.0f };

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    move = Vector3Add(move, forward);
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  move = Vector3Subtract(move, forward);
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) move = Vector3Add(move, rightH);
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  move = Vector3Subtract(move, rightH);
    if (IsKeyDown(KEY_SPACE))                     move.y += 1.0f; // ascend
    if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_C)) move.y -= 1.0f; // descend

    if (move.x != 0.0f || move.y != 0.0f || move.z != 0.0f) {
        move = Vector3Scale(Vector3Normalize(move), step);
        _camera.position = Vector3Add(_camera.position, move);
    }

    // ---- Wheel sets the fly speed (not zoom — there's no pivot to zoom to).
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        _flySpeed *= (1.0f + wheel * 0.12f);
        const float span = std::max(_map.getWidth(), _map.getHeight()) * TILE_SIZE;
        const float minS = span * 0.05f, maxS = span * 4.0f;
        if (_flySpeed < minS) _flySpeed = minS;
        if (_flySpeed > maxS) _flySpeed = maxS;
    }

    // ---- R: snap back to the overview (also drops follow).
    if (IsKeyPressed(KEY_R)) {
        _followedPlayer = -1;
        initCamera();
    }

    // ---- F: toggle riding along with the selected player.
    if (IsKeyPressed(KEY_F)) {
        if (_followedPlayer >= 0) {
            _followedPlayer = -1;
        } else if (_selectedX >= 0 && _selectedY >= 0) {
            const MapTile& tile = _map.getTile(_selectedX, _selectedY);
            if (!tile.players.empty()) {
                _followedPlayer = static_cast<std::int64_t>(tile.players.front().getId());
                _followAnchor   = { _selectedX * TILE_SIZE + TILE_SIZE / 2.0f, 0.0f,
                                    _selectedY * TILE_SIZE + TILE_SIZE / 2.0f };
            }
        }
    }

    // ---- Simulation speed: +/- request a new time unit from the server (sst).
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD))
        requestTimeUnit(_desiredFreq + (_desiredFreq < 10 ? 1 : 10));
    if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT))
        requestTimeUnit(_desiredFreq - (_desiredFreq <= 10 ? 1 : 10));

    // ---- Timeline: pause, scrub back/forward through recorded history, go live.
    if (IsKeyPressed(KEY_P))         togglePause();
    if (IsKeyPressed(KEY_PAGE_DOWN)) scrubBy(-1.0f); // back in time
    if (IsKeyPressed(KEY_PAGE_UP))   scrubBy(+1.0f); // forward in time
    if (IsKeyPressed(KEY_END))       goLive();

    // ---- Panels / music.
    if (IsKeyPressed(KEY_TAB))                     _showStats = !_showStats;
    if (IsKeyPressed(KEY_H) || IsKeyPressed(KEY_F1)) _showHelp = !_showHelp;
    if (isMusicTogglePressed())                    toggleMusic();

    // ---- Left-click (crosshair): select the aimed tile; double-click focuses.
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        pickTile();
        const double now = GetTime();
        if (_lastClickTime >= 0.0 && (now - _lastClickTime) < 0.30 &&
            _selectedX >= 0 && _selectedY >= 0) {
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
    const Vector3 centre = { tx * TILE_SIZE + TILE_SIZE / 2.0f, 0.0f,
                             ty * TILE_SIZE + TILE_SIZE / 2.0f };
    _camera.position = { centre.x, TILE_SIZE * 5.0f, centre.z + TILE_SIZE * 5.0f };
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
    for (auto& line : _net->poll()) {
        _history.push_back({ _elapsed, line });
        if (_live) {
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
    _map   = GameMap(w, h);
    _state = GuiState{};
    for (const auto& rec : _history) {
        if (rec.t <= t)
            _parser.apply(rec.line, _map, _state);
        else
            break; // _history is in arrival order, so the rest is in the future
    }
    _playT = t;
}

void Interface::togglePause()
{
    if (_live) {
        _live  = false;       // freeze on the current (latest) instant
        _playT = latestTime();
    } else {
        goLive();             // resume following the stream
    }
}

void Interface::scrubBy(float seconds)
{
    if (_history.empty())
        return;
    if (_live)
        _live = false; // first scrub drops out of live
    float t = _playT + seconds;
    if (t < 0.0f)            t = 0.0f;
    if (t > latestTime())    t = latestTime();
    rebuildWorldTo(t);
}

void Interface::goLive()
{
    // Catch the world up to everything recorded, then resume incremental apply.
    rebuildWorldTo(latestTime());
    _appliedIndex = _history.size();
    _playT        = latestTime();
    _live         = true;
}

void Interface::update()
{
    if (_musicLoaded)
        UpdateMusicStream(_backgroundMusic);

    _elapsed += GetFrameTime();

    // Record whatever the server pushed since last frame; in live mode it is
    // also folded into the world right away. Scrubbing replays from _history.
    recordIncoming();

    // Seed the speed control from the server's first reported time unit (sgt),
    // so +/- nudge from the real value instead of from zero.
    if (!_freqInit && _state.frequency > 0) {
        _desiredFreq = _state.frequency;
        _freqInit    = true;
    }

    // Camera ride-along: translate the eye by however far the followed player
    // moved since last frame, so free-look and free-fly still work while the
    // camera tracks it. Drop the follow silently if the player died/left.
    if (_followedPlayer >= 0) {
        auto it = _state.players.find(static_cast<std::uint32_t>(_followedPlayer));
        if (it == _state.players.end()) {
            _followedPlayer = -1;
        } else {
            const Vector3 now = { it->second.getX() * TILE_SIZE + TILE_SIZE / 2.0f, 0.0f,
                                  it->second.getY() * TILE_SIZE + TILE_SIZE / 2.0f };
            _camera.position = Vector3Add(_camera.position, Vector3Subtract(now, _followAnchor));
            _followAnchor    = now;
            updateCameraTarget();
        }
    }
}


namespace {
    constexpr float kTileHeight      = 2.0f;
    constexpr float kTileMargin      = 0.0f;
    constexpr float kPlayerBaseSize  = TILE_SIZE * 0.4f;
    constexpr float kPlayerY         = 4.0f;
    constexpr float kPlayerHeight    = 6.0f;
    constexpr float kTileTopY     = kTileHeight / 2.0f; // surface items stand on
    constexpr float kItemSpacing  = TILE_SIZE * 0.22f;  // grid pitch between stacked items
    constexpr int kMaxVisiblePlayers = 4;
    constexpr float kPlayerSpacing = TILE_SIZE * 0.28f;

    struct CountLabel { Vector3 worldPos; int count; Color color; };

    // Draw every tile of one checkerboard colour in a single batch. One
    // rlSetTexture per call (2 total for the floor) instead of one per tile, so
    // the GPU isn't forced to flush the batch 2500x/frame on a 50x50 map.
    // Chunked under the rlgl batch buffer so it stays correct on huge maps.
    void drawFloorBatched(Texture2D tex, bool wantDark, int w, int h)
    {
        if (tex.id == 0)
            return;

        const float size = TILE_SIZE - kTileMargin;
        const float half = size * 0.5f;
        const float y    = kTileTopY;
        const int   kQuadsPerBatch = 1500; // 4 verts each, well under the 8192 buffer

        rlSetTexture(tex.id);
        rlBegin(RL_QUADS);
        rlColor4ub(255, 255, 255, 255);
        rlNormal3f(0.0f, 1.0f, 0.0f);

        int emitted = 0;
        for (int ty = 0; ty < h; ++ty) {
            for (int tx = 0; tx < w; ++tx) {
                if (((tx + ty) % 2 == 0) != wantDark)
                    continue;
                if (emitted >= kQuadsPerBatch) {
                    rlEnd(); // flush, then resume (texture stays bound across this)
                    rlBegin(RL_QUADS);
                    rlColor4ub(255, 255, 255, 255);
                    rlNormal3f(0.0f, 1.0f, 0.0f);
                    emitted = 0;
                }
                float wx = tx * TILE_SIZE + TILE_SIZE / 2.0f;
                float wz = ty * TILE_SIZE + TILE_SIZE / 2.0f;
                rlTexCoord2f(0.0f, 1.0f); rlVertex3f(wx - half, y, wz - half);
                rlTexCoord2f(0.0f, 0.0f); rlVertex3f(wx - half, y, wz + half);
                rlTexCoord2f(1.0f, 0.0f); rlVertex3f(wx + half, y, wz + half);
                rlTexCoord2f(1.0f, 1.0f); rlVertex3f(wx + half, y, wz - half);
                ++emitted;
            }
        }

        rlEnd();
        rlSetTexture(0);
    }

    void drawTileOutline(float worldX, float worldZ, float size)
    {
        const float half = size * 0.5f;
        const float y = kTileTopY + 0.02f;

        DrawLine3D({ worldX - half, y, worldZ - half }, { worldX + half, y, worldZ - half }, BLACK);
        DrawLine3D({ worldX + half, y, worldZ - half }, { worldX + half, y, worldZ + half }, BLACK);
        DrawLine3D({ worldX + half, y, worldZ + half }, { worldX - half, y, worldZ + half }, BLACK);
        DrawLine3D({ worldX - half, y, worldZ + half }, { worldX - half, y, worldZ - half }, BLACK);
    }

    float playerOrientationAngle(Orientation orientation)
    {
        switch (orientation) {
            case Orientation::North: return 180.0f;
            case Orientation::East:  return 90.0f;
            case Orientation::South: return 0.0f;
            case Orientation::West:  return 270.0f;
        }
        return 0.0f;
    }
}


void Interface::render()
{
    std::vector<CountLabel> labels;

    BeginMode3D(_camera);

    // 360 background first, so the rest of the scene draws in front of it.
    drawSkybox();

    // Floor: two batched draws (dark + orange) instead of one per tile.
    const bool floorTextured = _darkTileTexture.id != 0 || _orangeTileTexture.id != 0;
    if (floorTextured) {
        drawFloorBatched(_darkTileTexture,   true,  _map.getWidth(), _map.getHeight());
        drawFloorBatched(_orangeTileTexture, false, _map.getWidth(), _map.getHeight());
    }

    // Frustum cull: skip the expensive per-tile content (outlines, players,
    // resource models) for tiles whose centre projects off-screen. The batched
    // floor above stays fully drawn — it's cheap and avoids holes at the edges.
    const Vector3 camFwd = Vector3Normalize(Vector3Subtract(_camera.target, _camera.position));
    const float   screenW    = static_cast<float>(GetScreenWidth());
    const float   screenH    = static_cast<float>(GetScreenHeight());
    const float   cullMargin = 160.0f; // px slack so partly-visible edge tiles still draw
    auto tileVisible = [&](float wx, float wz) {
        Vector3 c{ wx, kTileTopY, wz };
        Vector3 d{ c.x - _camera.position.x, c.y - _camera.position.y, c.z - _camera.position.z };
        if (d.x * camFwd.x + d.y * camFwd.y + d.z * camFwd.z <= 0.0f)
            return false; // behind the camera
        Vector2 sp = GetWorldToScreen(c, _camera);
        return sp.x >= -cullMargin && sp.x <= screenW + cullMargin
            && sp.y >= -cullMargin && sp.y <= screenH + cullMargin;
    };

    for (int y = 0; y < _map.getHeight(); ++y) {
        for (int x = 0; x < _map.getWidth(); ++x) {
            float worldX = x * TILE_SIZE + TILE_SIZE / 2.0f;
            float worldZ = y * TILE_SIZE + TILE_SIZE / 2.0f;

            if (!tileVisible(worldX, worldZ))
                continue;

            const MapTile& tile = _map.getTile(x, y);

            // Untextured fallback only (textured floor was drawn batched above).
            if (!floorTextured)
                DrawPlane({ worldX, kTileTopY, worldZ },
                          { TILE_SIZE - kTileMargin, TILE_SIZE - kTileMargin }, WHITE);
            drawTileOutline(worldX, worldZ, TILE_SIZE - kTileMargin);

            // Players: draw up to four robots side by side, then show a count label.
            const int playerCount = static_cast<int>(tile.players.size());
            if (playerCount > 0) {
                const int visiblePlayers = std::min(playerCount, kMaxVisiblePlayers);
                const int cols = visiblePlayers == 1 ? 1 : 2;
                const int rows = (visiblePlayers + cols - 1) / cols;
                const float originX = worldX - kPlayerSpacing * static_cast<float>(cols - 1) * 0.5f;
                const float originZ = worldZ - kPlayerSpacing * static_cast<float>(rows - 1) * 0.5f;

                for (int i = 0; i < visiblePlayers; ++i) {
                    const aiPlayer& player = tile.players[static_cast<size_t>(i)];
                    const int col = i % cols;
                    const int row = i / cols;
                    const float px = originX + kPlayerSpacing * static_cast<float>(col);
                    const float pz = originZ + kPlayerSpacing * static_cast<float>(row);

                    if (_playerModel.loaded) {
                        DrawModelEx(_playerModel.model, { px, kTileTopY, pz },
                                    { 0.0f, 1.0f, 0.0f }, playerOrientationAngle(player.getOrientation()),
                                    _playerModel.scale, WHITE);
                    } else {
                        DrawCube({ px, kPlayerY, pz },
                                  kPlayerBaseSize, kPlayerHeight, kPlayerBaseSize, YELLOW);
                    }
                }
                if (playerCount > kMaxVisiblePlayers)
                    labels.push_back({ { worldX, kPlayerY + kPlayerHeight, worldZ }, playerCount, YELLOW });
            }

            // Resources render only on empty tiles; occupied tiles show the player model alone.
            int totalItems = 0;
            if (playerCount == 0) {
                for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
                    totalItems += tile.resources[i];
            }

            if (playerCount == 0 && totalItems > 0) {
                const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(totalItems))));
                const int rows = (totalItems + cols - 1) / cols;
                // Shrink the pitch if the natural grid would spill past the tile,
                // so items stay on their own tile no matter the count.
                const float maxSpan = TILE_SIZE - kTileMargin;
                const float pitch   = std::min(kItemSpacing,
                                               maxSpan / static_cast<float>(std::max(cols, rows)));
                const float originX = worldX - pitch * (cols - 1) * 0.5f;
                const float originZ = worldZ - pitch * (rows - 1) * 0.5f;

                int slot = 0;
                for (int i = 0; i < MAP_RESOURCE_COUNT; ++i) {
                    const int count = tile.resources[i];
                    const bool hasModel = static_cast<size_t>(i) < _resourceModels.size()
                                        && _resourceModels[i].loaded;

                    for (int n = 0; n < count; ++n, ++slot) {
                        const int col = slot % cols;
                        const int row = slot / cols;
                        const float cx = originX + pitch * static_cast<float>(col);
                        const float cz = originZ + pitch * static_cast<float>(row);
                        const float angle = static_cast<float>((x * 53 + y * 97 + i * 17 + slot * 31) % 360);

                        if (hasModel) {
                            const ResourceModel& rm = _resourceModels[i];
                            DrawModelEx(rm.model, { cx, kTileTopY, cz }, { 0.0f, 1.0f, 0.0f }, angle,
                                        rm.scale, WHITE);
                        } else {
                            const float s = kItemWidth;
                            DrawCube({ cx, kTileTopY + s / 2.0f, cz }, s, s, s, RED);
                        }
                    }
                }
            }
        }
    }

    // Eggs: small cream spheres resting on the tile surface.
    for (const auto& [id, egg] : _state.eggs) {
        (void)id;
        if (egg.x < 0 || egg.y < 0 || egg.x >= _map.getWidth() || egg.y >= _map.getHeight())
            continue;
        float ex = egg.x * TILE_SIZE + TILE_SIZE / 2.0f;
        float ez = egg.y * TILE_SIZE + TILE_SIZE / 2.0f;
        if (!tileVisible(ex, ez))
            continue;
        DrawSphere({ ex, kTileTopY + TILE_SIZE * 0.08f, ez }, TILE_SIZE * 0.08f, BEIGE);
    }

    drawSelectionHighlight();

    EndMode3D();

    // Player head-count labels, projected to screen space now that we're out
    // of 3D mode. Resources are always drawn in full, so they need no label.
    for (const auto& label : labels) {
        Vector2 screenPos = GetWorldToScreen(label.worldPos, _camera);
        DrawText(TextFormat("x%d", label.count),
                 static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), 14, label.color);
    }

    if (_state.hasWinner)
        DrawText(TextFormat("WINNER: %s", _state.winner.c_str()),
                 GetScreenWidth() / 2 - 140, GetScreenHeight() / 2 - 20, 40, GOLD);

    drawHud();
    drawTimeline();
    drawTileInfoPanel();
    if (_showStats)
        drawStatsPanel();
    if (_showHelp)
        drawHelpOverlay();

    DrawFPS(GetScreenWidth() - 90, 10);
}

// ---------------------------------------------------------------------------
// Selection
// ---------------------------------------------------------------------------
void Interface::pickTile()
{
    // The cursor is captured (centred) in free-cam, so aim from the crosshair
    // at the middle of the screen rather than the frozen mouse position.
    Vector2 centre = { GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f };
    Ray ray = GetScreenToWorldRay(centre, _camera);
    if (std::fabs(ray.direction.y) < 1e-6f)
        return; // ray parallel to the ground, no hit

    // Intersect the tile-top plane y = kTileTopY.
    float t = (kTileTopY - ray.position.y) / ray.direction.y;
    if (t < 0.0f) {
        _selectedX = _selectedY = -1; // plane is behind the camera
        return;
    }

    float hx = ray.position.x + ray.direction.x * t;
    float hz = ray.position.z + ray.direction.z * t;
    int   tx = static_cast<int>(std::floor(hx / TILE_SIZE));
    int   ty = static_cast<int>(std::floor(hz / TILE_SIZE));

    if (tx >= 0 && ty >= 0 && tx < _map.getWidth() && ty < _map.getHeight()) {
        _selectedX = tx;
        _selectedY = ty;
    } else {
        _selectedX = _selectedY = -1; // clicked off the board -> deselect
    }
}

void Interface::drawSelectionHighlight()
{
    if (_selectedX < 0 || _selectedY < 0)
        return;

    float wx   = _selectedX * TILE_SIZE + TILE_SIZE / 2.0f;
    float wz   = _selectedY * TILE_SIZE + TILE_SIZE / 2.0f;
    float size = TILE_SIZE - kTileMargin;
    float half = size * 0.5f;

    // Translucent overlay + bright gold border just above the tile surface.
    DrawCube({ wx, kTileTopY + 0.15f, wz }, size, 0.3f, size, Color{ 255, 230, 0, 70 });

    float    y = kTileTopY + 0.35f;
    Vector3  a = { wx - half, y, wz - half };
    Vector3  b = { wx + half, y, wz - half };
    Vector3  c = { wx + half, y, wz + half };
    Vector3  d = { wx - half, y, wz + half };
    DrawLine3D(a, b, GOLD);
    DrawLine3D(b, c, GOLD);
    DrawLine3D(c, d, GOLD);
    DrawLine3D(d, a, GOLD);
}

void Interface::drawTileInfoPanel()
{
    if (_selectedX < 0 || _selectedY < 0)
        return;

    const MapTile& tile = _map.getTile(_selectedX, _selectedY);

    // Build the lines first so the panel can size itself.
    std::vector<std::pair<std::string, Color>> lines;
    lines.push_back({ TextFormat("Tile (%d, %d)", _selectedX, _selectedY), GOLD });

    int totalRes = 0;
    for (int i = 0; i < MAP_RESOURCE_COUNT; ++i) {
        int q = tile.resources[i];
        totalRes += q;
        if (q > 0)
            lines.push_back({ TextFormat("%-9s %d", std::string(MAP_RESOURCE_NAMES[i]).c_str(), q), RAYWHITE });
    }
    if (totalRes == 0)
        lines.push_back({ "(no resources)", GRAY });

    // Players standing on the tile, enriched with level/team from the registry.
    lines.push_back({ TextFormat("Players: %d", static_cast<int>(tile.players.size())), SKYBLUE });
    for (const aiPlayer& player : tile.players) {
        lines.push_back({ TextFormat("  #%u  lvl %d  %s", player.getId(), player.getLevel(),
                                     player.getTeam().c_str()), LIGHTGRAY });
    }

    // Eggs on this tile.
    int eggs = 0;
    for (const auto& [eid, egg] : _state.eggs) {
        (void)eid;
        if (egg.x == _selectedX && egg.y == _selectedY)
            ++eggs;
    }
    if (eggs > 0)
        lines.push_back({ TextFormat("Eggs: %d", eggs), BEIGE });

    // Panel geometry: top-right.
    const int pad    = 10;
    const int lineH  = 18;
    const int width  = 220;
    const int height = pad * 2 + static_cast<int>(lines.size()) * lineH;
    const int px     = GetScreenWidth() - width - 10;
    const int py     = 120;

    DrawRectangle(px, py, width, height, Color{ 0, 0, 0, 180 });
    DrawRectangleLines(px, py, width, height, GOLD);

    int ty = py + pad;
    for (const auto& [text, color] : lines) {
        DrawText(text.c_str(), px + pad, ty, 14, color);
        ty += lineH;
    }
}

// ---------------------------------------------------------------------------
// HUD / stats / help
// ---------------------------------------------------------------------------
void Interface::drawHud()
{
    // Compact always-on status, top-left.
    DrawText(TextFormat("Map %dx%d   Teams %d   Players %d   Eggs %d",
                        _map.getWidth(), _map.getHeight(),
                        static_cast<int>(_state.teams.size()),
                        static_cast<int>(_state.players.size()),
                        static_cast<int>(_state.eggs.size())),
             10, 10, 18, RAYWHITE);

    const int mm = static_cast<int>(_elapsed) / 60;
    const int ss = static_cast<int>(_elapsed) % 60;
    DrawText(TextFormat("Speed (time unit): %d  [+/- to change]   Elapsed %02d:%02d",
                        _state.frequency, mm, ss),
             10, 32, 16, SKYBLUE);

    if (_followedPlayer >= 0)
        DrawText(TextFormat("Following player #%lld  (F to release)",
                            static_cast<long long>(_followedPlayer)),
                 10, 52, 16, GOLD);

    // One-line control reminder; the full list is on H / F1.
    DrawText("ZQSD: fly   Mouse: look   Space/Ctrl: up/down   Wheel: speed   LMB: select   "
             "R: reset   F: follow   P: pause   Tab: stats   H: help",
             10, GetScreenHeight() - 24, 15, LIGHTGRAY);

    if (_net && _net->closed())
        DrawText("DISCONNECTED", GetScreenWidth() / 2 - 70, 10, 20, RED);

    // Crosshair: marks what a left click will select.
    const int cx = GetScreenWidth() / 2, cy = GetScreenHeight() / 2;
    DrawLine(cx - 8, cy, cx + 8, cy, Color{ 255, 255, 255, 160 });
    DrawLine(cx, cy - 8, cx, cy + 8, Color{ 255, 255, 255, 160 });
}

void Interface::drawStatsPanel()
{
    // --- Aggregate the whole environment from the live model. ---
    std::array<long, MAP_RESOURCE_COUNT> resTotals{};
    for (int y = 0; y < _map.getHeight(); ++y)
        for (int x = 0; x < _map.getWidth(); ++x) {
            const MapTile& t = _map.getTile(x, y);
            for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
                resTotals[i] += t.resources[i];
        }

    std::array<int, 8> levelCounts{}; // levels 1..8
    for (const auto& [id, p] : _state.players) {
        (void)id;
        int lvl = p.getLevel();
        if (lvl >= 1 && lvl <= 8)
            levelCounts[static_cast<size_t>(lvl - 1)]++;
    }

    // Build the lines first so the panel can size itself.
    std::vector<std::pair<std::string, Color>> lines;
    lines.push_back({ "GLOBAL STATS  (Tab)", GOLD });
    lines.push_back({ "Resources on map:", SKYBLUE });
    long grand = 0;
    for (int i = 0; i < MAP_RESOURCE_COUNT; ++i) {
        grand += resTotals[i];
        lines.push_back({ TextFormat("  %-9s %ld", std::string(MAP_RESOURCE_NAMES[i]).c_str(),
                                     resTotals[i]), RAYWHITE });
    }
    lines.push_back({ TextFormat("  %-9s %ld", "TOTAL", grand), LIGHTGRAY });

    lines.push_back({ "Players per team:", SKYBLUE });
    for (const auto& team : _state.teams) {
        int alive = 0, top = 0;
        for (const auto& [id, p] : _state.players) {
            (void)id;
            if (p.getTeam() == team) {
                ++alive;
                if (p.getLevel() > top) top = p.getLevel();
            }
        }
        lines.push_back({ TextFormat("  %-10s %d  (max lvl %d)", team.c_str(), alive, top),
                          RAYWHITE });
    }

    lines.push_back({ "Players per level:", SKYBLUE });
    std::string lvlLine = "  ";
    for (int l = 0; l < 8; ++l)
        lvlLine += TextFormat("L%d:%d  ", l + 1, levelCounts[static_cast<size_t>(l)]);
    lines.push_back({ lvlLine, RAYWHITE });

    lines.push_back({ TextFormat("Eggs: %d        Incantations: %d",
                                 static_cast<int>(_state.eggs.size()),
                                 static_cast<int>(_state.incanting.size())), BEIGE });
    lines.push_back({ TextFormat("Time unit: %d   Elapsed: %02d:%02d",
                                 _state.frequency,
                                 static_cast<int>(_elapsed) / 60,
                                 static_cast<int>(_elapsed) % 60), LIGHTGRAY });

    // Panel geometry: left side, under the HUD.
    const int pad    = 12;
    const int lineH  = 18;
    const int width  = 280;
    const int height = pad * 2 + static_cast<int>(lines.size()) * lineH;
    const int px     = 10;
    const int py     = 80;

    DrawRectangle(px, py, width, height, Color{ 0, 0, 0, 190 });
    DrawRectangleLines(px, py, width, height, GOLD);

    int ty = py + pad;
    for (const auto& [text, color] : lines) {
        DrawText(text.c_str(), px + pad, ty, 14, color);
        ty += lineH;
    }
}

void Interface::drawHelpOverlay()
{
    static const std::array<const char*, 16> kHelp = {
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
        "H / F1          this help",
    };

    const int pad   = 16;
    const int lineH = 22;
    const int width = 460;
    const int height = pad * 2 + static_cast<int>(kHelp.size()) * lineH;
    const int px = GetScreenWidth() / 2 - width / 2;
    const int py = GetScreenHeight() / 2 - height / 2;

    DrawRectangle(px, py, width, height, Color{ 0, 0, 0, 210 });
    DrawRectangleLines(px, py, width, height, GOLD);

    int ty = py + pad;
    for (size_t i = 0; i < kHelp.size(); ++i) {
        DrawText(kHelp[i], px + pad, ty, i == 0 ? 20 : 16, i == 0 ? GOLD : RAYWHITE);
        ty += lineH;
    }
}

void Interface::drawTimeline()
{
    const int   sw    = GetScreenWidth();
    const float last  = latestTime();
    const float frac  = last > 0.0f ? (_playT / last) : 1.0f;

    const int barW = sw / 2;
    const int barH = 8;
    const int x0   = (sw - barW) / 2;
    const int y0   = GetScreenHeight() - 52;

    // Mode label above the bar.
    if (_live) {
        DrawText("LIVE", x0, y0 - 20, 16, GREEN);
    } else {
        DrawText(TextFormat("PAUSED  %.1fs / %.1fs  (PageUp/Down scrub, End: live)",
                            _playT, last),
                 x0, y0 - 20, 16, GOLD);
    }

    // Track + filled portion + cursor knob.
    DrawRectangle(x0, y0, barW, barH, Color{ 0, 0, 0, 160 });
    DrawRectangle(x0, y0, static_cast<int>(barW * frac), barH,
                  _live ? Color{ 60, 200, 90, 200 } : Color{ 230, 190, 40, 220 });
    DrawRectangleLines(x0, y0, barW, barH, Color{ 255, 255, 255, 90 });
    const int knobX = x0 + static_cast<int>(barW * frac);
    DrawCircle(knobX, y0 + barH / 2, 5.0f, _live ? GREEN : GOLD);
}
