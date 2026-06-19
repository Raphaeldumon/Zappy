#pragma once

#include "raylibWrapper.hpp"
#include "gameMap.hpp"
#include <memory>
#include <string>

// Window defaults
inline constexpr int DEFAULT_WINDOW_WIDTH  = 1280;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 720;
inline constexpr std::string_view WINDOW_TITLE = "Zappy - Graphical Client";

// Tile rendering size in pixels
inline constexpr float TILE_SIZE = 64.0f;

class Interface {
public:
    Interface(int mapWidth, int mapHeight,
              int windowWidth  = DEFAULT_WINDOW_WIDTH,
              int windowHeight = DEFAULT_WINDOW_HEIGHT);
    ~Interface();

    // Main entry point: runs the game loop until the window is closed.
    void run();

    // Expose the map so external systems (e.g. network parser) can mutate it.
    GameMap& getMap();
    const GameMap& getMap() const;

private:
    // --- Core systems ---
    RaylibEngine _engine;
    GameMap      _map;

    // --- Camera ---
    Camera3D _camera;

    // --- Assets ---
    // Food model, loaded once and drawn at every food tile. When the .glb is
    // missing or fails to load we fall back to the old red cube.
    std::vector<Model> _resourceModels{};
    std::vector<float> _resourceScales{};
    bool _resourceModelsOk{false};

    // --- Internal loop steps ---
    void handleInput();
    void update();
    void render();

    // --- Helpers ---
    void initCamera();
    void loadResourceModels();
    void unloadResourceModels();
};