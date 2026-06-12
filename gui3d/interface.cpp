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
    loadFoodModel(); // needs the GL context, so after the engine init
}

Interface::~Interface()
{
    // Member destruction runs after this body, so the GL context (owned by
    // _engine) is still alive here — safe to free GPU resources.
    if (_foodModelOk)
        UnloadModel(_foodModel);
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
void Interface::loadFoodModel()
{
    // Path depends on where the binary is launched from: try both.
    const char* candidates[] = { "assets/monster.glb", "gui3d/assets/monster.glb" };
    for (const char* path : candidates) {
        if (!FileExists(path))
            continue;
        _foodModel = LoadModel(path);
        if (_foodModel.meshCount > 0) {
            _foodModelOk = true;
            break;
        }
        UnloadModel(_foodModel);
    }
    if (!_foodModelOk)
        return; // render() falls back to the red cube

    // Normalize: scale the model so its largest dimension spans ~45% of a tile,
    // whatever size it was authored at.
    BoundingBox box = GetModelBoundingBox(_foodModel);
    float ext = box.max.x - box.min.x;
    ext = std::max(ext, box.max.y - box.min.y);
    ext = std::max(ext, box.max.z - box.min.z);
    if (ext > 0.0001f)
        _foodScale = (TILE_SIZE * 0.45f) / ext;
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

void Interface::render()
{
    // Draw every tile as a flat cube.
    BeginMode3D(_camera);

    for (int y = 0; y < _map.getHeight(); ++y) {
        for (int x = 0; x < _map.getWidth(); ++x) {
            const MapTile& tile = _map.getTile(x, y);

            float worldX = x * TILE_SIZE + TILE_SIZE / 2.0f;
            float worldZ = y * TILE_SIZE + TILE_SIZE / 2.0f;

            // Alternate tile colours for a checkerboard grid.
            Color tileColor = ((x + y) % 2 == 0) ? DARKGREEN : GREEN;
            DrawCube({ worldX, 0.0f, worldZ }, TILE_SIZE - 2.0f, 2.0f, TILE_SIZE - 2.0f, tileColor);
            DrawCubeWires({ worldX, 0.0f, worldZ }, TILE_SIZE - 2.0f, 2.0f, TILE_SIZE - 2.0f, BLACK);

            // Highlight tiles that have at least one player.
            if (!tile.player_ids.empty()) {
                DrawCube({ worldX, 4.0f, worldZ }, TILE_SIZE * 0.4f, 6.0f, TILE_SIZE * 0.4f, YELLOW);
            }
            if (tile.resources[0]) {
                if (_foodModelOk) {
                    // Sit the model on the tile; per-tile rotation breaks up the
                    // grid repetition.
                    float angle = static_cast<float>((x * 53 + y * 97) % 360);
                    DrawModelEx(_foodModel, { worldX, 1.5f, worldZ }, { 0.0f, 1.0f, 0.0f }, angle,
                                { _foodScale, _foodScale, _foodScale }, WHITE);
                } else {
                    DrawCube({ worldX, 4.0f, worldZ }, TILE_SIZE * 0.4f, 6.0f, TILE_SIZE * 0.4f, RED);
                }
            }
            if (tile.resources[1]) {
                DrawCube({ worldX, 4.0f, worldZ }, TILE_SIZE * 0.4f, 6.0f, TILE_SIZE * 0.4f, BLUE);
            }
            if (tile.resources[2]) {
                DrawCube({ worldX, 4.0f, worldZ }, TILE_SIZE * 0.4f, 6.0f, TILE_SIZE * 0.4f, ORANGE);
            }
            if (tile.resources[3]) {
                DrawCube({ worldX, 4.0f, worldZ }, TILE_SIZE * 0.4f, 6.0f, TILE_SIZE * 0.4f, PURPLE);
            }
            if (tile.resources[4]) {
                DrawCube({ worldX, 4.0f, worldZ }, TILE_SIZE * 0.4f, 6.0f, TILE_SIZE * 0.4f, PINK);
            }
            if (tile.resources[5]) {
                DrawCube({ worldX, 4.0f, worldZ }, TILE_SIZE * 0.4f, 6.0f, TILE_SIZE * 0.4f, LIME);
            }
            if (tile.resources[6]) {
                DrawCube({ worldX, 4.0f, worldZ }, TILE_SIZE * 0.4f, 6.0f, TILE_SIZE * 0.4f, SKYBLUE);
            }
        }
    }

    EndMode3D();

    // HUD
    DrawText(TextFormat("Map: %dx%d", _map.getWidth(), _map.getHeight()), 10, 10, 20, RAYWHITE);
    DrawText("WASD / Arrows: pan   |   Scroll: zoom", 10, 35, 16, LIGHTGRAY);
    DrawFPS(DEFAULT_WINDOW_WIDTH - 80, 10);
}