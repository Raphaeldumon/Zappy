# P4 — Inès, GUI Dev — Gameplay & UX — Calendrier 20 jours

**Mission rappel** : scènes 2D/3D, HUD ImGui, menu, replay, caméras, audio, input, parser GUI protocol.

**Zones owned** : `gui/include/zappy/gui/{scene,ui,audio,input,net}/**`, `gui/assets/**`, `docs/01_architecture/03_gui_vulkan.md` (co-owner).

---

## S1 — Foundations

### D1 — lun 25/05
- Setup `gui/include/zappy/gui/{ui,audio,input,net,scene}/` headers vides
- Ajouter Dear ImGui via vcpkg
- Sample "ImGui hello" en standalone (binary test `gui_imgui_smoke`)
- `gui/assets/README.md` règles license + organisation
- **DoD** : ImGui smoke window OK
- Dépendances : fenêtre glfw de P3
- Bloque : aucun

### D2 — mar 26/05
- Intégrer ImGui dans `zappy_gui` principal (par dessus fenêtre vide)
- Input map glfw key callbacks (ESC, F3, Tab, F11)
- ADR-013 (ImGui docking)
- **DoD** : ImGui DemoWindow s'affiche, F3 toggle visible
- Dépendances : P3 init Vulkan
- Bloque : aucun

### D3 — mer 27/05 — M1
- Sync protocole 10h-11h
- `gui/include/zappy/gui/net/gui_client.hpp` parser GUI protocol line-based
- Mock GUI client consomme `.zrec` fixture
- ADR-014 (asset pipeline glTF+KTX2)
- **DoD** : `test_gui_protocol_parser.cpp` PASS
- Bloque : aucun

### D4 — jeu 28/05
- Caméra free orbit (mouse + WASD + zoom GLM)
- Camera class avec matrices view/proj
- Test `test_camera.cpp`
- **DoD** : peut tourner autour cube de P3, zoom
- Dépendances : cube de P3
- Bloque : aucun

### D5 — ven 29/05 — M2
- HUD ImGui squelette : top bar status, left teams panel, bottom timeline placeholder, docking saved
- Demo Friday + retro
- **DoD** : HUD layout visible, F11/F3/Tab fonctionnels
- Bloque : aucun

---

## S2 — Core MVP

### D6 — lun 1/06
- Sprint planning
- Torus mesh procedural (radius R + r, vertices generated CPU)
- Skybox cube procedural (placeholder gradient)
- Cam initial sur torus
- **DoD** : torus visible orbiting, skybox
- Dépendances : G-Buffer pass de P3

### D7 — mar 2/06
- Trantorian mesh (cube + orientation indicator), instancing N
- Resource mesh (gem-like, couleurs par type)
- 2D planisphère overlay (texture quad fullscreen)
- **DoD** : 50 trantorians + 200 ressources @ 60 FPS

### D8 — mer 3/06
- HUD ImGui v1 : Teams list (lit `tna`), Player Info (`pin`, `plv`, `ppo`), Timeline events
- Speed control widget → `sst T` au serveur
- **DoD** : HUD réactif events serveur

### D9 — jeu 4/06
- Caméra follow player (clic trantorien)
- Top-down camera (touche 1/2/3 switch)
- **DoD** : 3 modes caméra, transitions fluides

### D10 — ven 5/06 — M3
- Buffer + HUD polish
- Connexion live serveur `./zappy_gui -p 4242 -h localhost`
- Vérifier rendu live
- Demo Friday + retro
- **DoD** : GUI live connecté

---

## S3 — Bonus + intégration

### D11 — lun 8/06
- Sprint planning
- Skybox étoiles + nebula propre (FBM 3D coloré)
- Timeline ImGui : log événements (incantations, morts, broadcasts)
- **DoD** : ciel beau + timeline scroll réactive

### D12 — mar 9/06
- Debug panel F3 complet (frametime graph, GPU mem VMA, packets, draw calls, mesh count)
- **DoD** : F3 utile pour diagnostic

### D13 — mer 10/06
- Replay reader UI : timeline ImGui scrub, play/pause, speed control
- Load `.zrec`, parcours events
- **DoD** : replay fonctionnel, scrub à n'importe quel moment

### D14 — jeu 11/06
- Menu principal (Connect / Replay / Settings / Quit)
- Settings persistants JSON
- **DoD** : menu fonctionnel, settings save

### D15 — ven 12/06 — M4
- Buffer + animation trantorian (idle + walk)
- Demo Friday + retro
- **DoD** : trantorians visuellement vivants

---

## S4 — Polish + soutenance

### D16 — lun 15/06
- Sprint planning S4
- Animation skinned trantorian (walk, attack, death) si glTF rigging
- Particles refinement (incantation glow, ejection wave)
- **DoD** : visuels finaux

### D17 — mar 16/06
- Polish HUD final (icons, fonts, layout final)
- Audio mix final (volumes, ducking events)
- **DoD** : UX léchée

### D18 — mer 17/06 — Code freeze
- Matin : bug fixes
- 14h-17h : répétition + tournage vidéo (caméra cinematic mode)
- **DoD** : tag `v1.0.0-rc1`

### D19 — jeu 18/06
- Doc finale `03_gui_vulkan.md`
- Captures d'écran finales
- Slides "GUI Vulkan + Bonus" avec P3, P6
- Montage vidéo démo final 5 min (voiceover)
- 2ème répétition
- **DoD** : vidéo final monté

### D20 — ven 19/06 — Soutenance
- 9h-11h : préparation
- Soutenance : démo GUI live, fallback vidéo si crash
- **DoD** : soutenance livrée 🎉

---

## Sync points clés

| Quand | Avec qui | Sujet |
|-------|----------|-------|
| D1-D5 | P3 | Vulkan init / ImGui / frame graph pair |
| D3 | Tech-leads | M1 protocole |
| D7-D8 | P2 | Format messages GUI |
| D8 | P3 | Overlay pass setup |
| D11-D14 | P3 | Pair sur visuels |
| D16 | P3 | RT vs raster final |
| Chaque PR scene/ui/replay | P3 | Co-review |

## Outils / setup

- Vulkan SDK 1.3.x
- Blender (si modélisation Trantorian)
- Inkscape / GIMP (icons HUD)
- Audacity (édition audio)
- ffmpeg (montage vidéo démo)
- OBS Studio (capture vidéo démo)

## Auto-évaluation

| Sprint | Score (1-5) | Notes |
|--------|-------------|-------|
| S1 | __ | |
| S2 | __ | |
| S3 | __ | |
| S4 | __ | |
