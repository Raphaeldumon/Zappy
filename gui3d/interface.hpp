#pragma once

#include "raylibWrapper.hpp"
#include "gameMap.hpp"
#include <array>
#include <string>
#include <string_view>
#include <vector>

// Window defaults
inline constexpr int DEFAULT_WINDOW_WIDTH  = 1280;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 720;
inline constexpr std::string_view WINDOW_TITLE = "Zappy - Graphical Client";

// Edge length of a tile, expressed in 3D world units.
inline constexpr float TILE_SIZE = 64.0f;

class Interface {
public:
    Interface(int mapWidth, int mapHeight,
              int windowWidth  = DEFAULT_WINDOW_WIDTH,
              int windowHeight = DEFAULT_WINDOW_HEIGHT);
    ~Interface();

    // Owns the window and GPU models — duplicating it would double-free them.
    Interface(const Interface&)            = delete;
    Interface& operator=(const Interface&) = delete;
    Interface(Interface&&)                 = delete;
    Interface& operator=(Interface&&)      = delete;

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
    // One mesh per resource type, loaded once and drawn at every tile holding
    // that resource. The bounding box is cached so each instance can be scaled
    // to fit its grid cell and placed with its base sitting on the tile surface
    // (otherwise the mesh origin ends up buried below the map). When a .glb is
    // missing or fails to load we fall back to a small cube.
    struct ResourceModel {
        Model   model{};
        Vector3 scale{1.0f, 1.0f, 1.0f}; // per-axis scale baked at load so every model
                                         // ends up the same display size (see loadResourceModels)
        bool    loaded{false};
    };
    std::vector<ResourceModel> _resourceModels{};

    // --- Internal loop steps ---
    void handleInput();
    void update();
    void render();

    // --- Helpers ---
    void initCamera();
    void loadResourceModels();
    void unloadResourceModels();
};