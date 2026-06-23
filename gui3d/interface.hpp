#pragma once

#include "raylibWrapper.hpp"
#include "gameMap.hpp"
#include "guiState.hpp"
#include "netClient.hpp"
#include "protocolParser.hpp"
#include <array>
#include <memory>
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
    // Takes ownership of a NetClient that is already connected and past the
    // GRAPHIC handshake; mapWidth/mapHeight come from the server's msz.
    Interface(std::unique_ptr<NetClient> net, int mapWidth, int mapHeight,
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

    // --- Camera (orbit model) ---
    // The camera orbits a ground pivot (_camera.target). Its eye position is
    // derived every frame from yaw/pitch/distance, so all controls just nudge
    // these three scalars + the pivot — no more position/target drift.
    Camera3D _camera;
    float    _camYaw{0.0f};   // radians, rotation around the vertical axis
    float    _camPitch{0.0f}; // radians above the horizon (clamped)
    float    _camDist{0.0f};  // eye-to-pivot distance (zoom)

    // --- Lighting ---
    // One directional light + ambient term, applied to both the glb models
    // (via their material shader) and the immediate-mode cubes (via
    // BeginShaderMode). Loaded from assets/shaders; if that fails the scene
    // renders unlit (_lightingReady == false). Toggle live with the B key.
    Shader _lightShader{};
    Shader _defaultShader{};       // raylib's stock shader, to restore when toggled off
    int    _viewPosLoc{-1};
    bool   _lightingReady{false};  // shader compiled and bound
    bool   _lightingEnabled{true}; // user toggle (B)

    // --- Audio ---
    Music _backgroundMusic{};
    bool  _audioReady{false};
    bool  _musicLoaded{false};
    bool  _musicEnabled{true};

    // --- YEARS ---
    int _year{0};

    // --- Assets ---
    Texture2D _darkTileTexture{};
    Texture2D _orangeTileTexture{};

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

    // --- Networking ---
    std::unique_ptr<NetClient> _net;     // live server connection (post-handshake)
    ProtocolParser             _parser;  // applies wire lines to _map + _state
    GuiState                   _state;   // players, eggs, teams, winner

    // --- Selection ---
    // Tile picked by left-click; (-1,-1) = nothing selected. Drives the 3D
    // highlight and the top-right info panel.
    int _selectedX{-1};
    int _selectedY{-1};

    // --- Internal loop steps ---
    void handleInput();
    void update();
    void render();

    // --- Selection ---
    void pickTile();           // left-click ray -> _selectedX/_selectedY
    void drawSelectionHighlight(); // 3D outline on the picked tile (call inside Mode3D)
    void drawTileInfoPanel();  // 2D info panel, top-right (call outside Mode3D)

    // --- Helpers ---
    void initCamera();
    void updateCameraPosition(); // recompute eye from yaw/pitch/dist around target
    void loadLighting();
    void unloadLighting();
    void applyLightingToModels(bool on); // swap model material shaders for the B toggle
    void loadBackgroundMusic();
    void unloadBackgroundMusic();
    void toggleMusic();
    void loadTileTextures();
    void unloadTileTextures();
    void loadResourceModels();
    void unloadResourceModels();
};
