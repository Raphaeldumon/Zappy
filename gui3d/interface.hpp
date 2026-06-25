#pragma once

#include "raylibWrapper.hpp"
#include "gameMap.hpp"
#include "guiState.hpp"
#include "netClient.hpp"
#include "protocolParser.hpp"
#include <array>
#include <cstdint>
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

    // --- Camera (free-fly / spectator model) ---
    // A noclip free camera: _camera.position is the eye, yaw/pitch define the
    // look direction (the target is just eye + forward). The mouse looks around
    // (cursor captured), ZQSD flies along the view, Space/Shift go up/down and
    // the wheel sets the fly speed. Nothing orbits a pivot anymore.
    Camera3D _camera;
    float    _camYaw{0.0f};   // radians, heading around the vertical axis
    float    _camPitch{0.0f}; // radians, look up(+)/down(-), clamped near +-90deg
    float    _flySpeed{0.0f}; // world units / second, adjusted with the wheel

    // --- Audio ---
    Music _backgroundMusic{};
    bool  _audioReady{false};
    bool  _musicLoaded{false};
    bool  _musicEnabled{true};

    // --- Assets ---
    Texture2D _darkTileTexture{};
    Texture2D _orangeTileTexture{};

    // --- Skybox ---
    // A 360 equirectangular panorama sampled by view direction in a shader and
    // drawn on a camera-centred cube at the far plane, so it reads as an
    // infinitely distant background that pans correctly as you look around —
    // no pole pinch or seam like a textured sphere would give.
    Model     _skybox{};
    Texture2D _skyboxTex{};
    Shader    _skyShader{};
    bool      _skyboxLoaded{false};

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
    ResourceModel _playerModel{};

    // --- Player animation ---
    // Clips loaded from the player .glb, resolved by name to fixed slots so the
    // rest of the code refers to them by intent rather than by file order. Every
    // player on screen carries its own clip + frame and gets its pose uploaded
    // right before its own DrawModelEx, so they animate independently.
    enum class PlayerClip { Idle, Walk, Death, Kick, Dance, Pickup, Jump, Count };

    ModelAnimation* _playerAnims{nullptr};
    int             _playerAnimCount{0};
    // _clipIndex[(int)PlayerClip::X] = index into _playerAnims, or -1 if the .glb
    // has no animation with that name.
    std::array<int, static_cast<std::size_t>(PlayerClip::Count)> _clipIndex{};

    struct PlayerAnimState {
        PlayerClip loopClip{PlayerClip::Idle};  // looping base state: Idle / Walk / Dance
        float      loopFrame{0.0f};
        PlayerClip oneShot{PlayerClip::Count};  // active one-shot, Count = none
        float      oneShotFrame{0.0f};
        int        lastX{-9999};                 // last seen tile, to detect movement
        int        lastY{-9999};
        double     walkUntil{0.0};               // GetTime() until which Walk plays after a step
    };
    std::unordered_map<std::uint32_t, PlayerAnimState> _playerAnimState;

    // pdi erases the player before we can animate the death, so we spawn a ghost
    // that plays the Death clip once at the last known pose, then disappears.
    struct DeathGhost {
        float       x{0.0f};
        float       y{0.0f};
        Orientation orientation{Orientation::North};
        float       frame{0.0f};
    };
    std::vector<DeathGhost> _deathGhosts;

    // --- Networking ---
    std::unique_ptr<NetClient> _net;     // live server connection (post-handshake)
    ProtocolParser             _parser;  // applies wire lines to _map + _state
    GuiState                   _state;   // players, eggs, teams, winner

    // --- Selection ---
    // Tile picked by left-click; (-1,-1) = nothing selected. Drives the 3D
    // highlight and the top-right info panel.
    int _selectedX{-1};
    int _selectedY{-1};

    // --- Spectator state ---
    bool         _showStats{false};      // Tab: global environment stats panel
    bool         _showHelp{false};       // H / F1: full controls overlay
    std::int64_t _followedPlayer{-1};    // F: camera rides along this player id (-1 = none)
    Vector3      _followAnchor{};        // followed player's world pos last frame
    double       _lastClickTime{-1.0};   // for double-click (focus) detection
    int          _desiredFreq{0};        // time unit we last requested via sst
    bool         _freqInit{false};       // _desiredFreq seeded from the server yet?
    float        _elapsed{0.0f};         // wall-clock seconds since the GUI opened

    // --- Timeline (record + scrub) ---
    // Every state-changing protocol line is recorded with its arrival time, so
    // any past instant is reproducible by replaying the recorded lines onto a
    // fresh world. Live mode applies incrementally; scrubbing rebuilds.
    struct RecordedLine { float t; std::string line; };
    std::vector<RecordedLine> _history;
    bool        _live{true};       // following the live stream (vs. paused/scrubbing)
    float       _playT{0.0f};      // displayed timeline position when not live
    std::size_t _appliedIndex{0};  // history entries already folded into the live world

    // --- Internal loop steps ---
    void handleInput();
    void update();
    void render();

    // --- Selection ---
    void pickTile();               // left-click ray -> _selectedX/_selectedY
    void drawSelectionHighlight(); // 3D outline on the picked tile (call inside Mode3D)
    void drawTileInfoPanel();      // 2D info panel, top-right (call outside Mode3D)

    // --- Camera helpers ---
    void initCamera();
    void updateCameraTarget();        // recompute look target from eye + yaw/pitch
    void lookAt(Vector3 worldTarget); // set yaw/pitch so the eye faces a point
    void focusOnTile(int tx, int ty); // fly above a tile and look at it (double-click)

    // --- Spectator helpers ---
    void requestTimeUnit(int freq); // send "sst T" to the server (speed control)
    void drawHud();                 // permanent compact HUD (top-left)
    void drawStatsPanel();          // global environment stats (Tab)
    void drawHelpOverlay();         // full controls list (H / F1)

    // --- Timeline helpers ---
    void recordIncoming();          // drain socket into _history (+apply if live)
    void rebuildWorldTo(float t);   // reset world, replay recorded lines up to t
    void togglePause();             // P: live <-> frozen
    void scrubBy(float seconds);    // PageUp/PageDown: move the playback cursor
    void goLive();                  // End: catch up and resume the live stream
    void drawTimeline();            // bottom timeline bar (pos + mode)
    float latestTime() const { return _history.empty() ? 0.0f : _history.back().t; }

    // --- Asset lifecycle ---
    void loadBackgroundMusic();
    void unloadBackgroundMusic();
    void toggleMusic();
    void loadTileTextures();
    void unloadTileTextures();
    void loadSkybox();
    void unloadSkybox();
    void drawSkybox(); // call first inside BeginMode3D
    void loadResourceModels();
    void unloadResourceModels();
    void loadPlayerModel();
    void unloadPlayerModel();
    void loadPlayerAnimations();          // resolve clip names -> _clipIndex, log them
    void updatePlayerAnimations();        // per-frame anim state machine (call from update())
    void applyPlayerPose(std::uint32_t id); // upload a player's current clip+frame before draw
};
