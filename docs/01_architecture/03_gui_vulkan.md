# 03 — Architecture GUI Vulkan 1.3

## Périmètre fonctionnel

- Visualisation 2D **planisphère** (vue rectangulaire du torus déroulé)
- Visualisation 3D **planète torus** dans l'espace, étoiles, nebula, atmosphère
- Bascule de vue (`Tab`) : `2D / 3D / Split 2D+3D / Top-down stratège`
- **Caméras** : libre, follow player, top-down
- **Speed control** : 0.25x / 0.5x / 1x / 2x / 4x / 8x / 16x + pause + step-by-step
- **HUD ImGui** : team scores, inventaire player sélectionné, timeline events, broadcasts
- **Debug F3** : FPS, GPU memory, packets/sec, état du pipeline, frametime graph
- **Replay** : load `.zrec` et rejouer hors-ligne avec timeline scrub
- **Audio** : musique ambient + SFX
- **Menu** principal : connect to server / load replay / settings / quit

## Vulkan 1.3 — features activées

| Feature | Usage |
|---------|-------|
| `dynamic_rendering` | Plus de `VkRenderPass` legacy, on déclare `VkRenderingInfo` à chaque frame |
| `descriptor_indexing` (bindless) | 1 single descriptor set géant pour toutes les textures (skybox, models, ImGui font, etc.) |
| `compute_shader` | Particules GPU (incantations, étoiles, fumée, ejection) |
| `synchronization2` | Barriers simplifiées via `vkCmdPipelineBarrier2` |
| `ray_tracing_pipeline` *(optionnel)* | Reflets planète + ombres soft. Fallback obligatoire en raster. |
| `acceleration_structure` *(opt)* | TLAS/BLAS pour RT |
| `buffer_device_address` | Pour bindless buffers et RT |

## Frame graph (logique de rendu)

```
                  ┌──────────────┐
                  │  Frame begin │
                  └──────┬───────┘
                         │
                ┌────────▼─────────┐
                │  Update pass (CPU)
                │  - state diff to GPU
                │  - cameras matrices
                │  - particle emitters
                └────────┬─────────┘
                         │
                ┌────────▼─────────┐
                │  Shadow pass     │   (raster + cascaded shadow map du soleil)
                └────────┬─────────┘
                         │
                ┌────────▼─────────┐
                │  G-buffer pass   │   (deferred : albedo, normal, motion, depth)
                │  - skybox stars  │
                │  - planet torus  │
                │  - trantorians   │
                │  - resources     │
                └────────┬─────────┘
                         │
                ┌────────▼─────────┐
                │  Compute particles│
                └────────┬─────────┘
                         │
                ┌────────▼─────────────────┐
                │  Lighting pass           │
                │  - sun light             │
                │  - atmosphere scattering │
                │  - SSAO                  │
                │  - (optional) RT reflets │
                └────────┬─────────────────┘
                         │
                ┌────────▼─────────┐
                │  TAA resolve     │
                └────────┬─────────┘
                         │
                ┌────────▼─────────┐
                │  Bloom (compute) │
                └────────┬─────────┘
                         │
                ┌────────▼─────────┐
                │  Tonemap ACES    │
                └────────┬─────────┘
                         │
                ┌────────▼─────────┐
                │  2D overlay pass │
                │  (planisphere if│
                │   split-screen) │
                └────────┬─────────┘
                         │
                ┌────────▼─────────┐
                │  ImGui pass      │   (HUD + F3 debug)
                └────────┬─────────┘
                         │
                ┌────────▼─────────┐
                │  Present         │
                └──────────────────┘
```

## Layout sources

```
gui/
├── CMakeLists.txt
├── include/zappy/gui/
│   ├── app.hpp
│   ├── window.hpp
│   ├── renderer/
│   │   ├── vk_context.hpp           ← instance/device/queues via vk-bootstrap
│   │   ├── vk_swapchain.hpp
│   │   ├── vk_allocator.hpp         ← VMA wrapper
│   │   ├── vk_pipeline.hpp
│   │   ├── vk_shader.hpp            ← glslang runtime compile + cache
│   │   ├── vk_descriptor.hpp        ← bindless + push descriptors
│   │   ├── vk_buffer.hpp
│   │   ├── vk_image.hpp
│   │   ├── vk_sync.hpp
│   │   ├── frame_graph.hpp
│   │   ├── passes/
│   │   │   ├── shadow_pass.hpp
│   │   │   ├── gbuffer_pass.hpp
│   │   │   ├── particle_compute.hpp
│   │   │   ├── lighting_pass.hpp
│   │   │   ├── rt_reflection_pass.hpp
│   │   │   ├── taa_pass.hpp
│   │   │   ├── bloom_pass.hpp
│   │   │   ├── tonemap_pass.hpp
│   │   │   ├── overlay_2d_pass.hpp
│   │   │   └── imgui_pass.hpp
│   ├── scene/
│   │   ├── camera.hpp
│   │   ├── camera_modes.hpp
│   │   ├── world_view.hpp
│   │   ├── trantorian_mesh.hpp
│   │   ├── resource_mesh.hpp
│   │   ├── torus_mesh.hpp
│   │   ├── skybox.hpp
│   │   └── particles.hpp
│   ├── ui/
│   │   ├── hud.hpp
│   │   ├── menu_main.hpp
│   │   ├── debug_panel.hpp
│   │   ├── timeline.hpp
│   │   └── speed_control.hpp
│   ├── net/
│   │   ├── gui_client.hpp           ← parse protocole GUI
│   │   └── replay_reader.hpp
│   ├── audio/
│   │   └── audio_engine.hpp         ← miniaudio
│   └── input/
│       └── input_map.hpp            ← glfw → actions
├── src/...
├── shaders/                         ← .vert / .frag / .comp / .rgen / .rmiss / .rchit
│   ├── skybox.vert / skybox.frag
│   ├── torus.vert / torus.frag
│   ├── trantorian.vert / trantorian.frag
│   ├── resource.vert / resource.frag
│   ├── shadow.vert / shadow.frag
│   ├── particle.comp
│   ├── lighting.frag
│   ├── atmosphere.frag
│   ├── ssao.frag
│   ├── bloom_downsample.comp / bloom_upsample.comp
│   ├── tonemap.frag
│   ├── taa.frag
│   ├── overlay_2d.vert / overlay_2d.frag
│   ├── rt_reflection.rgen / .rmiss / .rchit
│   └── common/                      ← includes glsl
├── assets/                          ← gltf, ktx2, audio
└── tests/
```

## Shaders custom — détails

### Skybox étoiles + nebula
- Cubemap procedural via fragment shader
- Étoiles : hash 3D → Voronoi point pattern, intensité log-distribution
- Nebula : FBM (Fractal Brownian Motion) 3D coloré + gradient (rose/violet/cyan)
- Rotation lente du ciel

### Atmosphere scattering
- Rayleigh + Mie scattering integrated en ray-march fragment shader
- Sample depth map → calcul altitude → integration ligne de vue
- Param soleil : direction, intensité, couleur
- Halo lumineux autour du torus

### Water / lava (biomes)
- Si bonus biomes : certaines tiles ont type `water` ou `lava`
- Vertex shader anime hauteur via somme de sinusoides (gerstner waves)
- Fragment : normal map animée + reflet skybox + spéculaire

### Post-FX
- **Bloom** : downsample 6 mips compute → upsample additif
- **TAA** : reproject frame précédente + clip color box neighborhood
- **SSAO** : Hemisphere sampling 32 samples, blur cross 4x4
- **Tonemap** : ACES Filmic + gamma 2.2

### Ray tracing (optionnel)
- Pipeline RT pour reflets metalliques sur la planète + soft shadows
- Fallback : SSR (screen-space reflections) si pas de RT
- Compile time `#define ZAPPY_HAS_RT 1` driven by `VK_KHR_ray_tracing_pipeline` availability

## Caméras

```cpp
enum class CameraMode { FREE_ORBIT, FOLLOW_PLAYER, TOP_DOWN_STRATEGE, REPLAY_CINEMATIC };
```

- **FREE_ORBIT** : tourne autour du torus, WASD + souris + scroll zoom
- **FOLLOW_PLAYER** : click sur un trantorien → caméra le suit, smoothed
- **TOP_DOWN_STRATEGE** : ortho top-down, vue stratège
- **REPLAY_CINEMATIC** : interpole entre keyframes pré-définis pour la vidéo démo

## Speed control / time UI

- Slider exposant `[-1, 16]` log scale, valeur `<= 0` → pause
- Envoi de `sst T\n` au serveur pour changer le `f` (avec confirmation `sst T\n`)
- Step-by-step : envoie une seule unité de tick au serveur via socket admin (mode debug)
- En mode replay : modifie la vitesse de lecture du `.zrec` (multiplier sur le clock interne)

## HUD ImGui

Layout (ImGui docking) :
```
┌────────────────────────────────────────────────────────────────┐
│ Menu bar : File | View | Camera | Replay | Debug | Help        │
├────────────┬──────────────────────────────┬────────────────────┤
│ Teams (L) │                              │  Player Info (R)   │
│ - red 5p  │                              │  ID: 42            │
│ - blue 3p │     3D viewport / 2D map     │  Lvl: 4            │
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

## Debug panel F3 (toggle)

- Graphique frametime (60 derniers frames)
- GPU mem usage (VMA stats)
- Packets reçus/sec, bytes/sec
- Hierarchical mesh count, draw call count, triangle count
- Vulkan device info, validation layer status

## Pipeline d'assets

Format input :
- Models : `.gltf` 2.0
- Textures : `.ktx2` (KTX2 + Basis Universal compression)
- Audio : `.ogg` (musique), `.wav` (SFX)

Script `tools/build_assets.sh` :
1. Convertit `.obj/.fbx` → `.gltf` via assimp
2. Convertit `.png/.jpg` → `.ktx2` via `toktx`
3. Génère atlas si besoin
4. Copie dans `gui/assets/runtime/`

## Tests

- Tests headless impossible pour le rendu pur, mais :
  - Tests parser protocole GUI (lib `gui/net/gui_client`)
  - Tests replay reader (roundtrip)
  - Tests math (camera, frustum culling, torus → planisphère mapping)
  - Tests sync helpers (frame sync, queue submission)
- Smoke test CI : lance le GUI 5 sec contre un mock server → screenshot → compare diff vs reference (tolérance perceptual)
- Benchmark : frame timing par pass, regression-tested en CI nightly
