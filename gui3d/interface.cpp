#include "interface.hpp"
#include "raymath.h"

#include <algorithm>
#include <cmath>

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
    unloadResourceModels();
}


namespace {
    // Index-aligned with MapTile::resources / MAP_RESOURCE_COUNT.
    // std::array's size is checked at compile time against MAP_RESOURCE_COUNT below,
    // so a mismatch between the resource enum and this list fails to build instead
    // of corrupting memory at runtime.
    // One mesh per resource, index-aligned with MAP_RESOURCE_NAMES. Food has a
    // dedicated chicken mesh; the ore types use monster meshes whose embedded
    // texture identifies the mineral. NOTE: monster_lightPink.glb is byte-
    // identical to monster_black.glb, so phiras and linemate currently share a
    // mesh — drop in a distinct asset to tell them apart.
    constexpr std::array<const char*, MAP_RESOURCE_COUNT> kResourceModelPaths = {
        "assets/roast_chicken_HIGHRES.glb", // 0 food
        "assets/monster_black.glb",         // 1 linemate
        "assets/monster_blue.glb",          // 2 deraumere
        "assets/monster_golden.glb",        // 3 sibur
        "assets/monster_green.glb",         // 4 mendiane
        "assets/monster_lightPink.glb",     // 5 phiras
        "assets/monster_pink.glb"           // 6 thystame
    };

    // Optional base-colour texture override, index-aligned with the models above.
    // raylib only decodes PNG/JPEG when SUPPORT_FILEFORMAT_JPG is enabled, which
    // it is NOT by default — so the chicken's embedded JPEG base colour silently
    // fails to load and the mesh renders untextured. We ship a PNG copy of that
    // texture and bind it manually after LoadModel. nullptr = keep the mesh's
    // own embedded texture (the monsters use PNG and load fine).
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
                // arbitrary places (off to the side, above the mesh, etc.), which
                // is why some used to spawn off-tile or float/sink. Bake a
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
    constexpr float kTileTopY     = kTileHeight / 2.0f; // surface items stand on
    constexpr float kItemSpacing  = TILE_SIZE * 0.22f;  // grid pitch between stacked items

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

            // Resources: render the EXACT count of every item. Every model is
            // drawn at the SAME fixed world size (kItemSize) regardless of how
            // many share the tile, laid out on a centred square grid that stays
            // inside the tile. Models are pre-centred (see loadResourceModels),
            // so each one just drops onto its cell at the tile surface.
            int totalItems = 0;
            for (int i = 0; i < MAP_RESOURCE_COUNT; ++i)
                totalItems += tile.resources[i];

            if (totalItems > 0) {
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
    EndMode3D();

    // Player head-count labels, projected to screen space now that we're out
    // of 3D mode. Resources are always drawn in full, so they need no label.
    for (const auto& label : labels) {
        Vector2 screenPos = GetWorldToScreen(label.worldPos, _camera);
        DrawText(TextFormat("x%d", label.count),
                 static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), 14, label.color);
    }

    DrawText(TextFormat("Map: %dx%d", _map.getWidth(), _map.getHeight()), 10, 10, 20, RAYWHITE);
    DrawText("WASD / Arrows: pan   |   Scroll: zoom", 10, 35, 16, LIGHTGRAY);
    DrawFPS(GetScreenWidth() - 90, 10); // was hardcoded to DEFAULT_WINDOW_WIDTH
}