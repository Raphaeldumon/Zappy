# 03 — Architecture GUI Raylib

## Périmètre fonctionnel

- Visualisation 2D **planisphère** (vue rectangulaire du torus déroulé)
- Visualisation 3D **planète torus** dans l'espace, étoiles, nebula, atmosphère
- Bascule de vue (`Tab`) : `2D / 3D / Split 2D+3D / Top-down stratège`
- **Caméras** : libre, follow player, top-down
- **Speed control** : 0.25x / 0.5x / 1x / 2x / 4x / 8x / 16x + pause + step-by-step
- **HUD raygui** : team scores, inventaire player sélectionné, timeline events, broadcasts
- **Debug F3** : FPS, RAM usage, packets/sec, draw call count, frametime graph
- **Replay** : load `.zrec` et rejouer hors-ligne avec timeline scrub
- **Audio** : musique ambient + SFX
- **Menu** principal : connect to server / load replay / settings / quit

## Raylib — features utilisées

| Feature | Usage |
|---------|-------|
| `InitWindow / BeginDrawing` | Fenêtre + boucle de rendu principale |
| `Camera3D` | FREE_ORBIT, FOLLOW_PLAYER, TOP_DOWN via `UpdateCamera` |
| `DrawModel / DrawModelEx` | Torus planète, trantorians, resources (OBJ/GLTF) |
| `LoadShader / BeginShaderMode` | Post-FX GLSL : bloom, tonemap, atmosphere overlay |
| `RenderTexture2D` | Render-to-texture pour post-processing chain |
| `DrawSphereWires / DrawCube` | Debug primitives, resource items |
| `raygui` | HUD panels (teams, player info, timeline, speed control) |
| `PlaySound / PlayMusicStream` | SFX incantation/mort + musique ambient |
| `LoadTexture / DrawTexturePro` | Planisphère 2D, textures sol, skybox cubemap |

## Boucle de rendu

```
Frame begin (BeginDrawing)
│
├── Update (CPU)
│   ├── GuiClient::poll() → parse protocol lines → mise à jour Scene
│   ├── Camera update (UpdateCamera ou interpolation manuelle)
│   └── Particle system update (positions, durée de vie)
│
├── 3D pass (BeginMode3D)
│   ├── Skybox (cubemap ou shader procédural étoiles)
│   ├── Torus planète (DrawModel + shader atmosphere)
│   ├── Tiles / resources (DrawCube / DrawModel par tile)
│   ├── Players / trantorians (DrawModel + orientation)
│   ├── Eggs (DrawSphere)
│   └── Particles (incantations, éjections — DrawBillboard)
│
├── Post-FX (RenderTexture → shaders GLSL chaînés)
│   ├── Bloom (downsample + upsample via BeginShaderMode)
│   └── Tonemap ACES + gamma 2.2
│
├── 2D overlay (BeginMode2D si split-screen planisphère)
│   └── DrawTexturePro → vue rectangulaire déroulée
│
├── HUD (raygui)
│   ├── Teams panel (gauche)
│   ├── Player info panel (droite)
│   ├── Timeline events (bas)
│   └── Speed control bar
│
└── Frame end (EndDrawing)
```

## Layout sources

```
gui/
├── CMakeLists.txt
├── include/zappy/gui/
│   ├── app.hpp                     ← init + boucle principale
│   ├── renderer/
│   │   ├── raylib_app.hpp          ← run_raylib_app()
│   │   ├── post_fx.hpp             ← chaîne post-processing (RenderTexture2D)
│   │   ├── particle_system.hpp     ← particles CPU (incantations, éjections)
│   │   ├── world_renderer.hpp      ← draw torus, tiles, players, eggs
│   │   └── skybox.hpp              ← cubemap ou shader procédural étoiles
│   ├── scene/
│   │   ├── scene.hpp               ← TileView, PlayerView, Scene (inchangé)
│   │   ├── camera.hpp              ← Camera3D wrapper + modes
│   │   └── camera_modes.hpp
│   ├── ui/
│   │   ├── hud.hpp                 ← raygui panels
│   │   ├── menu_main.hpp
│   │   ├── debug_panel.hpp         ← F3 overlay
│   │   ├── timeline.hpp
│   │   └── speed_control.hpp
│   ├── net/
│   │   ├── gui_client.hpp          ← parse protocole GUI (inchangé)
│   │   └── replay_reader.hpp
│   ├── audio/
│   │   └── audio_engine.hpp        ← raylib InitAudioDevice + sounds
│   └── input/
│       └── input_map.hpp           ← IsKeyPressed → actions
├── src/
│   ├── main.cpp
│   ├── renderer/raylib_app.cpp
│   ├── renderer/post_fx.cpp
│   ├── renderer/particle_system.cpp
│   ├── renderer/world_renderer.cpp
│   ├── renderer/skybox.cpp
│   ├── scene/camera.cpp
│   ├── ui/hud.cpp
│   ├── ui/menu_main.cpp
│   ├── ui/debug_panel.cpp
│   ├── net/gui_client.cpp
│   └── audio/audio_engine.cpp
├── shaders/
│   ├── skybox.frag                 ← hash étoiles procédural + FBM nebula
│   ├── atmosphere.frag             ← Rayleigh+Mie overlay sur le torus
│   ├── bloom_downsample.frag
│   ├── bloom_upsample.frag
│   ├── tonemap.frag                ← ACES + gamma 2.2
│   ├── overlay_2d.frag             ← planisphère mapping
│   └── common/                     ← includes glsl partagés
├── assets/
│   ├── models/                     ← .obj (torus, trantorian, resources)
│   ├── textures/                   ← .png (sol, skybox faces, resource icons)
│   └── audio/                      ← .ogg (musique), .wav (SFX)
└── tests/
    ├── test_gui_protocol.cpp       ← tags protocole (inchangé)
    └── test_scene_update.cpp       ← vérif Scene après handle_line()
```

## Shaders custom — détails

### Skybox étoiles + nebula (GLSL fragment)
- Étoiles : hash 3D → Voronoi point pattern, intensité log-distribution
- Nebula : FBM (Fractal Brownian Motion) 3D coloré + gradient (rose/violet/cyan)
- Rendu sur fullscreen quad via `BeginShaderMode`

### Atmosphere overlay
- Rayleigh + Mie scattering en ray-march fragment shader
- Appliqué en post-pass sur la RenderTexture du pass 3D
- Param soleil : direction, intensité, couleur transmis via `SetShaderValue`

### Post-FX chain (RenderTexture2D)
- **Bloom** : downsample 4 mips → upsample additif (2 shaders GLSL)
- **Tonemap** : ACES Filmic + gamma 2.2

### Water / lava (optionnel biomes)
- Vertex shader via `DrawMeshInstanced` avec positions animées (gerstner waves)
- Fragment : normal map animée + reflet simplifié (cubemap lookup)

## Caméras

```cpp
enum class CameraMode { FREE_ORBIT, FOLLOW_PLAYER, TOP_DOWN_STRATEGE, REPLAY_CINEMATIC };
```

- **FREE_ORBIT** : `UpdateCamera(cam, CAMERA_ORBITAL)` ou custom ArcBall autour du torus
- **FOLLOW_PLAYER** : cible `camera.target = lerp(target, player.pos, dt * 5)`, smoothed
- **TOP_DOWN_STRATEGE** : `camera.projection = CAMERA_ORTHOGRAPHIC`, vue Z+ vers bas
- **REPLAY_CINEMATIC** : interpolation keyframes via `Vector3Lerp` + `QuaternionSlerp`

## Speed control / time UI

- `raygui` slider exposant [-1, 16] log scale, valeur ≤ 0 → pause
- Envoi de `sst T\n` au serveur pour changer le `f`
- Step-by-step : replay `.zrec` avance d'une entrée par appui touche
- En mode replay : multiplicateur vitesse sur clock interne (`GetFrameTime() * speed`)

## HUD raygui

Layout (panneaux raygui ancrés) :
```
┌────────────────────────────────────────────────────────────────┐
│ Menu bar : File | View | Camera | Replay | Debug | Help        │
├────────────┬──────────────────────────────┬────────────────────┤
│ Teams (L)  │                              │  Player Info (R)   │
│ - red 5p   │                              │  ID: 42            │
│ - blue 3p  │     3D viewport / 2D map     │  Lvl: 4            │
│ - ...      │                              │  Inv: food 12, ..  │
│ ...        │                              │  Pos: (10,7)       │
├────────────┴──────────────────────────────┴────────────────────┤
│ Timeline events (bottom dock) :                                │
│ [t=124s] red#3 elevation success lvl 3 → 4                     │
│ [t=130s] blue#1 broadcast "help north"                         │
├────────────────────────────────────────────────────────────────┤
│ Speed: ◀ ‖ ▶ [—————●———] 4x      Status: connected localhost   │
└────────────────────────────────────────────────────────────────┘
```

Implémentation : `GuiPanel`, `GuiLabel`, `GuiSlider`, `GuiListView` de raygui.h.

## Debug panel F3 (toggle)

- `DrawFPS` + frametime graph (ring buffer 60 valeurs → `DrawLineStrip`)
- RAM usage via `/proc/self/status` (Linux) ou `GetMemoryInfo` stub
- Packets reçus/sec, bytes/sec (compteurs dans `GuiClient`)
- Draw call count (compteur manuel incrémenté dans `world_renderer.cpp`)

## Pipeline d'assets

Format input (simplifié vs Vulkan) :
- Models : `.obj` (simple) ou `.gltf` 2.0 via `LoadModel`
- Textures : `.png` ou `.jpg` via `LoadTexture`
- Audio : `.ogg` (musique via `LoadMusicStream`), `.wav` (SFX via `LoadSound`)

Script `tools/build_assets.sh` :
1. Convertit `.fbx/.blend` → `.obj` via Blender headless ou assimp-cli
2. Optimise textures `.png` avec `optipng`
3. Copie dans `gui/assets/`

Pas de KTX2/Basis compression nécessaire.

## Build

```cmake
# CMakeLists.txt — détection Raylib
find_package(raylib QUIET)
if(NOT raylib_FOUND)
    find_package(PkgConfig QUIET)
    pkg_check_modules(RAYLIB QUIET raylib)
endif()

if(raylib_FOUND OR RAYLIB_FOUND)
    # build réel
    target_sources(zappy_gui PRIVATE src/renderer/raylib_app.cpp ...)
    target_compile_definitions(zappy_gui PRIVATE ZAPPY_GUI_HAS_RAYLIB=1)
    target_link_libraries(zappy_gui PRIVATE raylib)
else()
    # stub banner (comportement actuel inchangé)
    message(STATUS "  gui: raylib not found -> building STUB zappy_gui")
endif()
```

Install : `sudo apt install libraylib-dev` ou vcpkg `raylib`.

## Tests

- Tests parser protocole GUI (`tests/test_gui_protocol.cpp`) — inchangés, pas de rendu
- Tests mise à jour scène (`tests/test_scene_update.cpp`) — feed `handle_line()`, vérif `Scene`
- Tests caméras (`tests/test_camera.cpp`) — math interpolation ArcBall, frustum
- Smoke test CI : lance GUI 3 sec en mode replay `.zrec` headless (`-headless` flag) → vérifie exit 0
- Pas de screenshot diff (trop fragile en CI), mais frame counter vérifié > 0
