# P3 — Sami, GUI Lead — Vulkan core — Calendrier 20 jours

**Mission rappel** : architecture Vulkan, frame graph, dynamic rendering, bindless, compute particles, ray tracing optionnel, post-FX.

**Zones owned** : `gui/include/zappy/gui/renderer/**`, `gui/src/renderer/**`, `gui/shaders/**` (co-owner avec P4), `docs/01_architecture/03_gui_vulkan.md`.

---

## S1 — Foundations

### D1 — lun 25/05
- Initialiser `gui/CMakeLists.txt`, deps vcpkg (`vulkan-headers`, `vk-bootstrap`, `vma`, `glfw3`, `glm`, `glslang`)
- `gui/src/main.cpp` ouvre fenêtre glfw 800x600 noire
- Doc Vulkan SDK setup dans `docs/09_appendix/01_useful_commands.md`
- **DoD** : `./zappy_gui` ouvre fenêtre, ESC ferme
- Dépendances : aucune
- Bloque : P4 (ImGui sample dépend de la fenêtre)

### D2 — mar 26/05
- Sample triangle Vulkan (Sascha Willems `01_triangle`)
- Init via vk-bootstrap (instance, physical device, queue, swapchain)
- Dynamic rendering (pas de render pass legacy)
- ADR-002 (dynamic rendering)
- Validation layers ON
- **DoD** : triangle coloré rendu, 0 warning validation
- Bloque : P4 (caméra orbite sur sample)

### D3 — mer 27/05 — M1
- Sync protocole 10h-11h
- Évoluer sample triangle → cube texturé
- VMA wrapper basique (`AllocatedBuffer`, `AllocatedImage`)
- Hot-reload shader glslang (timestamp watcher)
- ADR-012 (shader hot-reload)
- **DoD** : cube rendu, modif `.frag` → recompile auto
- Bloque : P4 (camera tourne autour cube)

### D4 — jeu 28/05
- Frame graph squelette : abstraction `IPass`, `FrameGraph::execute()`
- Premier `GBufferPass` minimal (draw cube en offscreen, compose)
- ADR-011 (frame graph design)
- **DoD** : RenderDoc montre 2 passes distinctes
- Bloque : P4 (overlay pass pour ImGui)

### D5 — ven 29/05 — M2
- Bindless descriptor indexing (1 set géant)
- Sample 2 cubes texturés avec bindless
- Doc dans `03_gui_vulkan.md`
- Demo Friday + retro
- **DoD** : RenderDoc 1 set + 2 textures, M2 PASS
- Bloque : P4 (assets / textures multiples)

---

## S2 — Core MVP

### D6 — lun 1/06
- Sprint planning
- Pipeline G-buffer + Lighting pass deferred basique
- Sun light directionnel
- Multi-mesh draw (torus + cubes placeholders trantorian)
- **DoD** : scène 3D lighting basique, 60 FPS @ 1080p
- Bloque : P4 (scene depend de pipeline)

### D7 — mar 2/06
- Pass LightingPass complet
- Premier shader skybox étoiles (FBM noise hash) preview
- **DoD** : skybox étoilée brille
- Bloque : P4 (a besoin pour assembler scène)

### D8 — mer 3/06
- ShadowPass (cascaded shadow maps)
- OverlayPass 2D pour ImGui
- **DoD** : ombres soft, HUD ImGui visible
- Bloque : P4 (HUD dépend de overlay)

### D9 — jeu 4/06
- Particles compute shader (GPU only)
- Émetteur CPU → GPU buffer → compute simulation → vertex shader rendering
- **DoD** : ~10k particules @ 60 FPS, particules à la mort
- Bloque : aucun (effet visuel)

### D10 — ven 5/06 — M3
- Smoke test CI (xvfb headless, capture screenshot, compare reference)
- Établir reference image
- Demo Friday + retro
- **DoD** : smoke test < 5% pixel diff

---

## S3 — Bonus + intégration

### D11 — lun 8/06
- Sprint planning
- Atmosphere scattering shader (Rayleigh+Mie ray-march)
- Sun direction param + halo lumineux
- **DoD** : atmosphère visible autour torus
- Bloque : aucun

### D12 — mar 9/06
- SSAO pass basique (32 samples, blur 4x4)
- TAA pass basique (reproject + clip)
- **DoD** : SSAO + TAA actifs, before/after visible

### D13 — mer 10/06
- Tonemap ACES + Bloom (downsample/upsample compute)
- **DoD** : pipeline post-FX 80% complet

### D14 — jeu 11/06
- LOD trantorian (3 niveaux) + frustum culling
- Optimisation draw calls
- **DoD** : 60 FPS @ 1440p avec 100+ trantorians

### D15 — ven 12/06 — M4
- Buffer + investigation RT setup (pour S4)
- Doc shaders
- Demo Friday + retro, ADR-007 (RT GO/NO-GO)
- **DoD** : M4 PASS, ADR-007 décidé

---

## S4 — Polish + soutenance

### D16 — lun 15/06
- Sprint planning S4
- Si ADR-007 GO : RT pipeline implementation (reflection torus metallic)
- TLAS/BLAS construction, `.rgen`/`.rmiss`/`.rchit` shaders
- **DoD** : reflets RT visibles
- Si NO-GO : SSR fallback final + autre polish

### D17 — mar 16/06
- TAA final tuning (history reject, neighborhood clip)
- Bloom final tuning
- Tonemap ACES propre
- **DoD** : post-FX excellent, reference screenshots final

### D18 — mer 17/06 — Code freeze
- Matin : bug fixes
- 14h-17h : Répétition complète
- Tournage cinematic vidéo démo (parallèle)
- **DoD** : tag `v1.0.0-rc1`

### D19 — jeu 18/06
- Doc finale `03_gui_vulkan.md`
- README GUI + captures
- Slides "GUI Vulkan + Bonus" avec P6
- 2ème répétition
- Montage vidéo cinematic
- **DoD** : doc et slides reviewed

### D20 — ven 19/06 — Soutenance
- 9h-11h : préparation
- Soutenance : Slides driver + Q&A Vulkan rendering
- **DoD** : soutenance livrée 🎉

---

## Sync points clés

| Quand | Avec qui | Sujet |
|-------|----------|-------|
| D1 | P4 | Init Vulkan / ImGui |
| D2-D5 | P4 | Pair sur frame graph |
| D3 | Tech-leads | M1 protocole |
| D8 | P4 | Overlay pour ImGui |
| D11-D14 | P4 | Pair sur post-FX visuels |
| D16 | P4 | RT design |
| D17 | P4 | Reference images |
| Chaque PR shaders | P4 | Co-review |

## Outils / setup

- Vulkan SDK 1.3.x
- RenderDoc (frame analysis)
- Nsight Graphics (si GPU NVIDIA)
- Tracy (profiling)
- vkguide.dev + Sascha Willems samples comme reference

## Auto-évaluation

| Sprint | Score (1-5) | Notes |
|--------|-------------|-------|
| S1 | __ | |
| S2 | __ | |
| S3 | __ | |
| S4 | __ | |
