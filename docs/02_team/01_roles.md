# 01 — Rôles détaillés des 6 personnes

Les **noms** ci-dessous sont des placeholders : remplacez par les vrais noms de l'équipe.

| ID | Nom (placeholder) | Rôle | Pôle |
|----|-------------------|------|------|
| P1 | Léa | **Server Lead** | Serveur |
| P2 | Marc | **Server Dev — Réseau & Ops** | Serveur |
| P3 | Sami | **GUI Lead — Vulkan core** | GUI |
| P4 | Inès | **GUI Dev — Gameplay & UX** | GUI |
| P5 | Théo | **AI Lead — RL training** | IA |
| P6 | Yanis | **AI Dev — Simulateur & DevOps** | IA + Infra |

---

## P1 — Léa, Server Lead

### Mission
Concevoir et maintenir le **cœur de la logique serveur** : état du monde, rules du jeu, time loop, persistance.

### Zones de code owned (CODEOWNERS)
- `server/core/**`
- `tests/integration_yaml/**` (co-owner avec P5)
- `docs/01_architecture/02_server.md`, `docs/01_architecture/05_simulator.md`

### Livrables clés
- World state (tile, player, egg, resource, vision cone, wrap toroidal)
- Event scheduler (priority queue, tick determinism)
- Game rules (elevation conditions, vision, broadcast directionality, fork)
- Persistance (snapshot JSON, reload SIGUSR1/SIGUSR2)
- Hot-reload config (densités, f, lifespan)
- Tests Catch2 ≥ 70% coverage sur `server/core/`

### Skills attendues
- C++17/20 strong
- Game state machine / event-driven
- Tests unitaires Catch2
- Capacité à écrire des invariants (assertions, debug mode)

### Décisions à porter (ADRs)
- ADR-001 (langage serveur), ADR-005 (.zrec format), refactor core/runtime
- Format snapshot disque (JSON vs binaire)

### Autorité / escalation
- Tech-lead du pôle serveur : arbitre les conflits techniques server/sim
- Escalade vers les leads des autres pôles si conflit de contrat protocole

---

## P2 — Marc, Server Dev — Réseau & Ops

### Mission
Couche réseau, parsing protocole AI/GUI, mode admin, observabilité (Prometheus), recorder `.zrec`.

### Zones de code owned
- `server/runtime/network_layer.{hpp,cpp}`
- `server/runtime/protocol_*.{hpp,cpp}`
- `server/runtime/recorder.{hpp,cpp}`
- `server/runtime/metrics.{hpp,cpp}`
- `grafana/**`, `prometheus/**`
- `docs/01_architecture/06_protocols.md`

### Livrables clés
- Poll/asio loop conforme sujet (single thread, no busy wait)
- Parser AI protocol + dispatcher
- Emitter GUI protocol (push events, no full state re-broadcast)
- Protocole admin (token auth, commandes runtime)
- Recorder `.zrec` write
- Prometheus exporter HTTP /metrics
- Dashboard Grafana JSON fonctionnel
- Tests parser ≥ 80% coverage

### Skills attendues
- C++17/20
- Réseau bas niveau (poll/epoll/asio)
- Sécurité basique (auth token, sanitization input)

### Décisions à porter (ADRs)
- ADR-009 : asio standalone vs boost vs raw poll
- ADR-010 : format métriques Prometheus

---

## P3 — Sami, GUI Lead — Vulkan core

### Mission
Architecture Vulkan : init, frame graph, dynamic rendering, bindless, compute, ray tracing optionnel, post-FX.

### Zones de code owned
- `gui/include/zappy/gui/renderer/**`
- `gui/src/renderer/**`
- `gui/shaders/**` (co-owner avec P4 pour les shaders gameplay)
- `docs/01_architecture/03_gui_vulkan.md`

### Livrables clés
- Initialisation Vulkan via vk-bootstrap (instance, device, queues, swapchain)
- VMA wrapper (buffer, image allocation)
- Frame graph orchestrant les passes
- Dynamic rendering (zéro VkRenderPass legacy)
- Bindless descriptor set (toutes textures dans 1 set)
- Compute particles (incantations, étoiles, fumée)
- Pass : shadow, gbuffer, lighting, atmosphere, ssao, taa, bloom, tonemap
- RT optional pipeline (avec fallback raster obligatoire)
- Hot-reload shaders glslang
- Tests math (camera, frustum, torus mapping)
- Smoke test CI : 5 sec rendu vs reference image

### Skills attendues
- Vulkan 1.x **expert**
- GLSL, mémoire GPU, barriers, synchronization
- Frame graph patterns

### Décisions à porter (ADRs)
- ADR-002 (dynamic rendering)
- ADR-007 (RT fallback strategy)
- ADR-011 (frame graph design)
- ADR-012 (shader hot-reload)

---

## P4 — Inès, GUI Dev — Gameplay & UX

### Mission
Tout ce que voit et fait l'utilisateur : scènes 3D/2D, HUD, menus, replay, caméras, audio, input.

### Zones de code owned
- `gui/include/zappy/gui/scene/**`
- `gui/include/zappy/gui/ui/**`
- `gui/include/zappy/gui/audio/**`
- `gui/include/zappy/gui/input/**`
- `gui/include/zappy/gui/net/**`
- `gui/assets/**`
- `docs/01_architecture/03_gui_vulkan.md` (co-owner)

### Livrables clés
- Scène : torus mesh, trantorian mesh, resource mesh, skybox, particles
- HUD ImGui complet (teams, inventory, timeline, broadcasts)
- Debug panel F3
- Menu principal
- Caméras (free, follow, top-down, cinematic)
- Speed control 0.25x..16x + pause + step
- Replay reader (.zrec) + timeline scrub
- Audio (music ambient + sfx) via miniaudio
- Input map (key + mouse)
- Parser GUI protocol côté client
- Pipeline assets (build_assets.sh)
- Tests parser GUI + math caméra

### Skills attendues
- C++17/20
- ImGui experience appréciée
- Sens UX, layout, ergonomie
- Asset pipeline (gltf, ktx2)

### Décisions à porter (ADRs)
- ADR-013 : ImGui docking layout
- ADR-014 : asset pipeline glTF + KTX2
- ADR-015 : replay timeline UX

---

## P5 — Théo, AI Lead — RL training

### Mission
Pipeline RL multi-agent : environnement PettingZoo, RLlib PPO, reward shaping, curriculum, self-play, ELO, exports.

### Zones de code owned
- `ai_python/zappy_train/**`
- `ai_python/tests/**`
- `ai_python/notebooks/**`
- `docs/01_architecture/04_ai_rl.md`

### Livrables clés
- Env PettingZoo `ParallelEnv` wrappant `zappy_sim`
- Encodage obs/action/reward
- Config PPO RLlib (YAML)
- Pipeline self-play + ELO
- Curriculum learning (stages)
- Eval pipeline (vs `zappy_ref`, win rate)
- Export TorchScript (.pt)
- Dashboards W&B / Streamlit
- Tests pytest + mutmut sur reward/obs/encoders

### Skills attendues
- Python expert
- PyTorch + RL (PPO, multi-agent)
- RLlib ou Ray
- Reward shaping experience

### Décisions à porter (ADRs)
- ADR-004 (RLlib vs SB3)
- ADR-016 (curriculum design)
- ADR-017 (reward shaping)
- ADR-018 (export format)

---

## P6 — Yanis, AI Dev — Simulateur & DevOps

### Mission
Pont C++ ↔ Python (pybind11), inférence libtorch C++ dans `zappy_ai`, infra CI/CD, Docker, devcontainer, GitHub Pages docs.

### Zones de code owned
- `ai_cpp/**`
- `sim_python/**`
- `docker/**`
- `.github/workflows/**`
- `.pre-commit-config.yaml`
- `tools/**` (co-owner)
- `docs/01_architecture/05_simulator.md` (co-owner avec P1)

### Livrables clés
- Bindings pybind11 pour `libzappy_sim`
- Binaire `zappy_ai` C++ avec libtorch inférence
- Fallback rule-based FSM
- Broadcast codec team
- Dockerfile + Dockerfile.training + devcontainer
- CI complète (build matrix, lint, tests, bench, deploy docs)
- Pre-commit hooks
- Asset build script
- Bench runner
- Tests cpp pour rule-based + broadcast codec
- Conformance test sim vs runtime

### Skills attendues
- C++17/20
- Python + pybind11
- Docker, CI GitHub Actions
- Bash scripting

### Décisions à porter (ADRs)
- ADR-003 (pybind11 strategy)
- ADR-006 (libtorch vs ONNX)
- ADR-008 (broadcast codec)
- ADR-019 (CI strategy)

---

## Profondeur & spécialisation

Chacun est **expert** dans sa zone, mais doit pouvoir reviewer les PR adjacentes :
- P1 ↔ P2 : reviews server cross-team
- P3 ↔ P4 : reviews GUI cross-team
- P5 ↔ P6 : reviews AI cross-team
- P1 + P6 : co-reviews sur `core/` (impacts simulateur)
- P3 + P6 : co-reviews sur `libtorch` integration
- P2 + P6 : co-reviews sur CI / Docker / metrics

Voir [02_raci.md](02_raci.md) pour la matrice RACI complète.
