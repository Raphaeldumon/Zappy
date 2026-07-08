#pragma once

#include "gameMap.hpp"
#include "environment.hpp"
#include "guiState.hpp"
#include "netClient.hpp"
#include "protocolParser.hpp"
#include "raylibWrapper.hpp"
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// Window defaults
inline constexpr int DEFAULT_WINDOW_WIDTH = 1920;
inline constexpr int DEFAULT_WINDOW_HEIGHT = 950;
inline constexpr std::string_view WINDOW_TITLE = "Zappy - Graphical Client";

// Edge length of a tile, expressed in 3D world units.
inline constexpr float TILE_SIZE = 64.0f;

class Interface
{
  public:
    // Takes ownership of a NetClient that is already connected and past the
    // GRAPHIC handshake; mapWidth/mapHeight come from the server's msz.
    Interface(std::unique_ptr<NetClient> net, int mapWidth, int mapHeight, int windowWidth = DEFAULT_WINDOW_WIDTH,
              int windowHeight = DEFAULT_WINDOW_HEIGHT);
    ~Interface();

    // Owns the window and GPU models — duplicating it would double-free them.
    Interface(const Interface &) = delete;
    Interface &operator=(const Interface &) = delete;
    Interface(Interface &&) = delete;
    Interface &operator=(Interface &&) = delete;

    // Main entry point: runs the game loop until the window is closed.
    void run();

    // Expose the map so external systems (e.g. network parser) can mutate it.
    GameMap &getMap();
    const GameMap &getMap() const;

  private:
    // --- Core systems ---
    RaylibEngine _engine;
    GameMap _map;

    // --- Camera (free-fly / spectator model) ---
    // A noclip free camera: _camera.position is the eye, yaw/pitch define the
    // look direction (the target is just eye + forward). The mouse looks around
    // (cursor captured), ZQSD flies along the view, Space/Shift go up/down and
    // the wheel sets the fly speed. Nothing orbits a pivot anymore.
    gfx::Camera _camera;
    float _camYaw{0.0f};   // radians, heading around the vertical axis
    float _camPitch{0.0f}; // radians, look up(+)/down(-), clamped near +-90deg
    float _flySpeed{0.0f}; // world units / second, adjusted with the wheel

    // --- Audio ---
    gfx::MusicHandle _backgroundMusic{gfx::NoHandle};
    bool _audioReady{false};
    bool _musicLoaded{false};
    bool _musicEnabled{true};

    // --- Assets ---
    gfx::TextureHandle _darkTileTexture{gfx::NoHandle};
    gfx::TextureHandle _orangeTileTexture{gfx::NoHandle};
    gfx::TextureHandle _grassTileTexture{gfx::NoHandle};
    gfx::TextureHandle _blackGrassTileTexture{gfx::NoHandle};

    // --- Skybox ---
    // A 360 equirectangular panorama sampled by view direction in a shader and
    // drawn on a camera-centred cube at the far plane, so it reads as an
    // infinitely distant background that pans correctly as you look around —
    // no pole pinch or seam like a textured sphere would give. The cube, shader
    // and texture are owned by the facade (RaylibEngine); we just ask it to
    // load and draw the skybox.

    // One mesh per resource type, loaded once and drawn at every tile holding
    // that resource. The bounding box is cached so each instance can be scaled
    // to fit its grid cell and placed with its base sitting on the tile surface
    // (otherwise the mesh origin ends up buried below the map). When a .glb is
    // missing or fails to load we fall back to a small cube.
    struct ResourceModel
    {
        gfx::ModelHandle handle{gfx::NoHandle};
        gfx::Vec3 scale{1.0f, 1.0f, 1.0f}; // per-axis scale baked at load so every model
                                           // ends up the same display size (see loadResourceModels)
        bool loaded{false};
    };
    std::vector<ResourceModel> _resourceModels{};
    ResourceModel _playerModel{};

    // --- Player animation ---
    // Clips loaded from the player .glb, resolved by name to fixed slots so the
    // rest of the code refers to them by intent rather than by file order. Every
    // player on screen carries its own clip + frame and gets its pose uploaded
    // right before its own DrawModelEx, so they animate independently.
    enum class PlayerClip
    {
        Idle,
        Walk,
        Death,
        Kick,
        Dance,
        Pickup,
        Jump,
        Count
    };

    gfx::AnimSetHandle _playerAnims{gfx::NoHandle};
    int _playerAnimCount{0};
    // _clipIndex[(int)PlayerClip::X] = index into the anim set, or -1 if the .glb
    // has no animation with that name.
    std::array<int, static_cast<std::size_t>(PlayerClip::Count)> _clipIndex{};

    struct PlayerAnimState
    {
        PlayerClip loopClip{PlayerClip::Idle}; // looping base state: Idle / Walk / Dance
        float loopFrame{0.0f};
        PlayerClip oneShot{PlayerClip::Count}; // active one-shot, Count = none
        float oneShotFrame{0.0f};
        int lastX{-9999}; // last seen tile, to detect movement
        int lastY{-9999};
        // Cell-to-cell glide. The logical tile jumps the instant ppo arrives; the
        // body instead slides at constant speed from where it was (fromX/fromY) to
        // that tile across moveProgress 0->1. The Walk clip is driven by the SAME
        // moveProgress, so the legs and the ground advance together (no foot
        // sliding) and one stride lands per cell. progress == 1 means settled.
        float dispX{0.0f}; // current displayed pos, fractional tile units
        float dispY{0.0f};
        float fromX{0.0f}; // where the current step started
        float fromY{0.0f};
        float moveProgress{1.0f}; // 0..1 across the active step
        bool posInit{false};      // guards the first-frame seed
    };
    std::unordered_map<std::uint32_t, PlayerAnimState> _playerAnimState;

    // pdi erases the player before we can animate the death, so we spawn a ghost
    // that plays the Death clip once at the last known pose, then disappears.
    struct DeathGhost
    {
        float x{0.0f};
        float y{0.0f};
        Orientation orientation{Orientation::North};
        float frame{0.0f};
    };
    std::vector<DeathGhost> _deathGhosts;

    // --- Networking ---
    std::unique_ptr<NetClient> _net; // live server connection (post-handshake)
    ProtocolParser _parser;          // applies wire lines to _map + _state
    GuiState _state;                 // players, eggs, teams, winner

    // --- Selection ---
    // Tile picked by left-click; (-1,-1) = nothing selected. Drives the 3D
    // highlight and the top-right info panel.
    int _selectedX{-1};
    int _selectedY{-1};

    // --- Hover (crosshair aim) ---
    // Tile currently under the crosshair, recomputed every frame; (-1,-1) =
    // aiming off the board. Drives the faint 3D outline and the tooltip.
    int _hoverX{-1};
    int _hoverY{-1};

    // --- Event feed ---
    // Rolling log of narrative events (broadcasts, deaths, elevations...),
    // drained from GuiState::feedEvents each frame and stamped with arrival
    // time so entries can fade out with age.
    struct FeedEntry
    {
        float t;          // _elapsed when the event arrived
        gfx::Color color; // per-kind accent, baked at drain time
        std::string text; // fully formatted, ready to draw
    };
    std::deque<FeedEntry> _feed;

    // --- Speech bubbles ---
    // One active bubble per broadcasting player, keyed by id; expires after a
    // few seconds. Drawn projected above the player's head.
    struct Bubble
    {
        std::string text;
        float until; // _elapsed after which the bubble disappears
    };
    std::unordered_map<std::uint32_t, Bubble> _bubbles;

    // --- Random events ---
    // Meteorites are driven by the authoritative server through `smg meteor`
    // events; the GUI keeps only the active fall/glow animations.
    struct Meteorite
    {
        int x{0};
        int y{0};
        float age{0.0f}; // seconds since spawn
    };
    std::vector<Meteorite> _meteorites;
    ResourceModel _meteoriteModel{};
    ResourceModel _incantationModel{};
    void loadMeteoriteModel();
    void unloadMeteoriteModel();
    void loadIncantationModel();
    void unloadIncantationModel();
    void updateRandomEvents(float dt); // roll the dice + advance active events
    void drawMeteorites();             // call inside Mode3D

    // --- Torus view ---
    // T switches the world between the flat grid and a real torus: the map
    // wraps on both axes, so the donut is its honest shape. Every world
    // position goes through surfaceAt(), which either passes flat coordinates
    // through unchanged or wraps them around the torus and hands back a local
    // frame (up/right/back) for orienting models on the curved surface.
    bool _torusView{false};

    struct Surface
    {
        gfx::Vec3 pos;   // world position, already lifted to the tile surface
        gfx::Vec3 up;    // outward surface normal
        gfx::Vec3 right; // tangent along increasing map x
        gfx::Vec3 back;  // tangent along increasing map y
    };
    struct TorusGeom
    {
        float R; // major radius (around the hole)
        float r; // minor radius (tube)
        gfx::Vec3 c; // world-space centre
    };
    TorusGeom torusGeom() const;
    // (wx, wz) are flat world coords (tile * TILE_SIZE ...), h is the height
    // above the tile surface. Works with fractional coords, so mid-step player
    // interpolation wraps seamlessly across the map seam in torus mode.
    Surface surfaceAt(float wx, float wz, float h) const;
    void toggleTorusView();
    void drawTorusFloor(); // checkerboard painted on the torus (call inside Mode3D)
    // Tile border at height h above the surface; curvature-subdivided on the torus.
    void drawTileEdges(int x, int y, float h, gfx::Color c);

    // --- Perf overlay (F3) ---
    // Per-frame render counters, reset at the top of render(), so every
    // optimisation (culling, pose bucketing, instancing, LOD) is measurable.
    struct FrameStats
    {
        int tilesDrawn{0};
        int tilesCulled{0};
        int players{0};     // player models drawn (incl. death ghosts)
        int poseUploads{0}; // CPU skinning uploads (<= players thanks to bucketing)
        int itemsModel{0};  // resources drawn as instanced meshes
        int itemsLod{0};    // resources drawn as far-away LOD cubes
        int eggs{0};
    };
    FrameStats _stats;
    bool _showPerf{false};
    void drawPerfOverlay();

    // --- Spectator state ---
    bool _showStats{false};           // Tab: global environment stats panel
    bool _showHelp{false};            // H / F1: full controls overlay
    bool _weatherVisible{true};       // V: visual weather/season overlay

    // --- Environnement (saison / météo / jour-nuit) ---
    env::EnvironmentState _env;   // source unique des paramètres visuels ambiants
    int _forceSeason{-1};         // F5: index dans kDebugSeasons, -1 = serveur
    int _forceWeather{-1};        // F6: index dans kDebugWeathers, -1 = serveur
    bool _endHidden{false};           // Enter: dismiss the end screen to keep using the GUI
    bool _mouseCaptured{true};        // Esc releases; click inside captures for free-look
    float _endScroll{0.0f};           // mouse-wheel offset for the end-screen player list
    std::int64_t _followedPlayer{-1}; // F: camera rides along this player id (-1 = none)
    gfx::Vec3 _followAnchor{};        // followed player's world pos last frame
    double _lastClickTime{-1.0};      // for double-click (focus) detection
    int _desiredFreq{0};              // time unit we last requested via sst
    bool _freqInit{false};            // _desiredFreq seeded from the server yet?
    float _elapsed{0.0f};             // wall-clock seconds since the GUI opened

    // --- Timeline (record + scrub) ---
    // Every state-changing protocol line is recorded with its arrival time, so
    // any past instant is reproducible by replaying the recorded lines onto a
    // fresh world. Live mode applies incrementally; scrubbing rebuilds.
    struct RecordedLine
    {
        float t;
        std::string line;
    };
    std::vector<RecordedLine> _history;
    bool _live{true};             // following the live stream (vs. paused/scrubbing)
    float _playT{0.0f};           // displayed timeline position when not live
    std::size_t _appliedIndex{0}; // history entries already folded into the live world

    // --- Internal loop steps ---
    void handleInput();
    void update();
    void render();

    // --- Selection ---
    bool aimedTile(int &tx, int &ty) const; // crosshair ray -> tile; false if off-board
    void pickTile();                        // left-click: aimedTile -> _selectedX/_selectedY
    void drawSelectionHighlight();          // 3D outline on the picked tile (call inside Mode3D)
    void drawHoverHighlight();              // faint outline on the aimed tile (call inside Mode3D)
    void drawIncantationRings();            // pulsing rings on incanting tiles (call inside Mode3D)
    void drawTileInfoPanel();               // 2D info panel, top-right (call outside Mode3D)
    void drawHoverTooltip();                // compact tile summary near the crosshair

    // --- Event feed / bubbles ---
    void drainFeedEvents();   // GuiState::feedEvents -> _feed + _bubbles (call from update())
    void drawEventFeed();     // scrolling log, bottom-left, age-faded
    void drawSpeechBubbles(); // broadcast text above player heads

    // --- Camera helpers ---
    void initCamera();
    void updateCameraTarget();          // recompute look target from eye + yaw/pitch
    void lookAt(gfx::Vec3 worldTarget); // set yaw/pitch so the eye faces a point
    void focusOnTile(int tx, int ty);   // fly above a tile and look at it (double-click)

    // --- Spectator helpers ---
    void requestTimeUnit(int freq);                      // send "sst T" to the server (speed control)
    void drawHud();                                      // permanent compact HUD (top-left)
    void drawWeatherOverlay();                           // visual weather pass under the HUD
    void drawStatsPanel();                               // global environment stats (Tab)
    void drawHelpOverlay();                              // full controls list (H / F1)
    void drawEndScreen();                                // centered winner summary after seg
    gfx::Color teamColor(const std::string &team) const; // palette by team slot (max 8)

    // --- Timeline helpers ---
    void recordIncoming();        // drain socket into _history (+apply if live)
    void rebuildWorldTo(float t); // reset world, replay recorded lines up to t
    void togglePause();           // P: live <-> frozen
    void scrubBy(float seconds);  // PageUp/PageDown: move the playback cursor
    void goLive();                // End: catch up and resume the live stream
    void drawTimeline();          // bottom timeline bar (pos + mode)
    float latestTime() const
    {
        return _history.empty() ? 0.0f : _history.back().t;
    }

    // --- Asset lifecycle ---
    void loadBackgroundMusic();
    void unloadBackgroundMusic();
    void toggleMusic();
    void loadTileTextures();
    void unloadTileTextures();
    void loadResourceModels();
    void unloadResourceModels();
    void loadPlayerModel();
    void unloadPlayerModel();
    void loadPlayerAnimations();   // resolve clip names -> _clipIndex, log them
    void updatePlayerAnimations(); // per-frame anim state machine (call from update())
    // A player's current clip + frame, for the render-time pose bucketing
    // (false when the model has no animations or the player has no state yet).
    bool playerPose(std::uint32_t id, int &clipIdx, float &frame) const;
};
