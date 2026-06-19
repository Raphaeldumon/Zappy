#include "interface.hpp"

#include <algorithm>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Interface::Interface(int mapWidth, int mapHeight, int windowWidth, int windowHeight)
    : _engine(windowWidth, windowHeight, std::string(WINDOW_TITLE))
    , _map(mapWidth, mapHeight)
{
    initCamera();
    loadResourceModels(); // needs the GL context, so after the engine init
}

Interface::~Interface()
{
    // Member destruction runs after this body, so the GL context (owned by
    // _engine) is still alive here — safe to free GPU resources.
    for (auto& model : _resourceModels) {
        if (model.meshCount > 0)
            UnloadModel(model);
    }
}


namespace {
    // Index-aligned with MapTile::resources / MAP_RESOURCE_COUNT.
    // std::array's size is checked at compile time against MAP_RESOURCE_COUNT below,
    // so a mismatch between the resource enum and this list fails to build instead
    // of corrupting memory at runtime.
    // NOTE: only index 0 (food) has a dedicated model right now — indices 1-6
    // are placeholder monster meshes standing in for ore types. Swap these out
    // once the real mineral models exist.
    constexpr std::array<const char*, MAP_RESOURCE_COUNT> kResourceModelPaths = {
        "assets/roast_chiken_HIGHRES.glb",
        "assets/monster_black.glb",
        "assets/monster_blue.glb",
        "assets/monster_golden.glb",
        "assets/monster_green.glb",
        "assets/monster_lightPink.glb",
        "assets/monster_pink.glb"
    };

    float computeNormalizedScale(const Model& model, float targetSize)
    {
        BoundingBox box = GetModelBoundingBox(model);
        float ext = std::max({
            box.max.x - box.min.x,
            box.max.y - box.min.y,
            box.max.z - box.min.z
        });
        return (ext > 0.0001f) ? (targetSize / ext) : 1.0f;
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
    _resourceScales.reserve(kResourceModelPaths.size());

    for (const char* path : kResourceModelPaths) {
        Model model = { 0 };
        float scale = 1.0f;

        if (!FileExists(path)) {
            TraceLog(LOG_WARNING, "loadResourceModels: missing asset '%s' (falling back to cube)", path);
        } else {
            model = LoadModel(path);
            if (model.meshCount <= 0) {
                TraceLog(LOG_WARNING, "loadResourceModels: failed to load '%s' (falling back to cube)", path);
            } else {
                scale = computeNormalizedScale(model, TILE_SIZE * 0.45f);
                _resourceModelsOk = true;
            }
        }

        _resourceModels.push_back(model);
        _resourceScales.push_back(scale);
    }
}

void Interface::initCamera()
{
    // Isometric-ish top-down view centered on the map.
    float centerX = (_map.getWidth()  * TILE_SIZE) / 2.0f;
    float centerZ = (_map.getHeight() * TILE_SIZE) / 2.0f;

    _camera.position   = { centerX, 300.0f, centerZ + 200.0f }; // eye
    _camera.target     = { centerX, 0.0f,   centerZ };           // look-at
    _camera.up         = { 0.0f,    1.0f,   0.0f };              // world up
    _camera.fovy       = 45.0f;
    _camera.projection = CAMERA_PERSPECTIVE;
}

// ---------------------------------------------------------------------------
// Loop steps
// ---------------------------------------------------------------------------
void Interface::handleInput()
{
    // Camera pan with arrow keys / WASD
    const float PAN_SPEED = 4.0f;

    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
        _camera.position.x += PAN_SPEED;
        _camera.target.x   += PAN_SPEED;
    }
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
        _camera.position.x -= PAN_SPEED;
        _camera.target.x   -= PAN_SPEED;
    }
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
        _camera.position.z -= PAN_SPEED;
        _camera.target.z   -= PAN_SPEED;
    }
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
        _camera.position.z += PAN_SPEED;
        _camera.target.z   += PAN_SPEED;
    }

    // Zoom with mouse wheel
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        _camera.position.y -= wheel * 15.0f;
        if (_camera.position.y < 50.0f) _camera.position.y = 50.0f;
        if (_camera.position.y > 800.0f) _camera.position.y = 800.0f;
    }
}

void Interface::update()
{
    // Game logic / network tick goes here.
}


namespace {
    constexpr float kTileHeight      = 2.0f;
    constexpr float kTileMargin      = 2.0f;
    constexpr float kPlayerBaseSize  = TILE_SIZE * 0.4f;
    constexpr float kPlayerY         = 4.0f;
    constexpr float kPlayerHeight    = 6.0f;
    constexpr float kResourceBaseY   = 1.5f;
    constexpr float kResourceYStep   = 0.2f;
    constexpr int   kMaxStackPerType = 3; // models drawn before we switch to a "xN" label
    constexpr float kJitterX[kMaxStackPerType] = { 0.0f,  0.22f, -0.22f };
    constexpr float kJitterZ[kMaxStackPerType] = { 0.0f, -0.22f,  0.22f };

    struct CountLabel { Vector3 worldPos; int count; Color color; };
}


void Interface::render()
{
    std::vector<CountLabel> labels;

    BeginMode3D(_camera);
    for (int y = 0; y < _map.getHeight(); ++y) {
        for (int x = 0; x < _map.getWidth(); ++x) {
            const MapTile& tile = _map.getTile(x, y);
            float worldX = x * TILE_SIZE + TILE_SIZE / 2.0f;
            float worldZ = y * TILE_SIZE + TILE_SIZE / 2.0f;

            Color tileColor = ((x + y) % 2 == 0) ? DARKGREEN : GREEN;
            DrawCube({ worldX, 0.0f, worldZ }, TILE_SIZE - kTileMargin, kTileHeight, TILE_SIZE - kTileMargin, tileColor);
            DrawCubeWires({ worldX, 0.0f, worldZ }, TILE_SIZE - kTileMargin, kTileHeight, TILE_SIZE - kTileMargin, BLACK);

            // Players: marker grows with headcount instead of being fixed-size.
            const int playerCount = static_cast<int>(tile.player_ids.size());
            if (playerCount > 0) {
                float bump = 1.0f + 0.15f * static_cast<float>(std::min(playerCount - 1, 5));
                DrawCube({ worldX, kPlayerY, worldZ },
                          kPlayerBaseSize * bump, kPlayerHeight, kPlayerBaseSize * bump, YELLOW);
                if (playerCount > 1)
                    labels.push_back({ { worldX, kPlayerY + kPlayerHeight, worldZ }, playerCount, YELLOW });
            }

            // Resources: draw up to kMaxStackPerType offset instances so a
            // stack actually reads as "more than one"; beyond that, a count label.
            for (int i = 0; i < MAP_RESOURCE_COUNT; ++i) {
                int count = tile.resources[i];
                if (count <= 0) continue;

                int shown = std::min(count, kMaxStackPerType);
                bool hasModel = static_cast<size_t>(i) < _resourceModels.size()
                              && _resourceModels[i].meshCount > 0;

                for (int s = 0; s < shown; ++s) {
                    float angle = static_cast<float>((x * 53 + y * 97 + i * 17 + s * 31) % 360);
                    Vector3 pos = {
                        worldX + kJitterX[s] * TILE_SIZE,
                        kResourceBaseY + i * kResourceYStep,
                        worldZ + kJitterZ[s] * TILE_SIZE
                    };

                    if (hasModel) {
                        float scale = _resourceScales[i];
                        DrawModelEx(_resourceModels[i], pos, { 0.0f, 1.0f, 0.0f }, angle,
                                    { scale, scale, scale }, WHITE);
                    } else {
                        DrawCube({ pos.x, kPlayerY, pos.z }, TILE_SIZE * 0.25f, kPlayerHeight, TILE_SIZE * 0.25f, RED);
                    }
                }

                if (count > kMaxStackPerType)
                    labels.push_back({ { worldX, kResourceBaseY + i * kResourceYStep + 2.0f, worldZ }, count, WHITE });
            }
        }
    }
    EndMode3D();

    // Count labels for stacks that got capped, projected to screen space now
    // that we're out of 3D mode.
    for (const auto& label : labels) {
        Vector2 screenPos = GetWorldToScreen(label.worldPos, _camera);
        DrawText(TextFormat("x%d", label.count),
                 static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), 14, label.color);
    }

    DrawText(TextFormat("Map: %dx%d", _map.getWidth(), _map.getHeight()), 10, 10, 20, RAYWHITE);
    DrawText("WASD / Arrows: pan   |   Scroll: zoom", 10, 35, 16, LIGHTGRAY);
    DrawFPS(GetScreenWidth() - 90, 10); // was hardcoded to DEFAULT_WINDOW_WIDTH
}