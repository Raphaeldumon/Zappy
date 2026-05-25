# Sprint 1 (W1) — Foundations

**Dates** : lundi 25 mai 2026 → vendredi 29 mai 2026
**Thème** : Foundations — socle commun, infra, contrats. **Aucune feature gameplay** ne démarre avant la fin du sprint.

## Goals du sprint

1. **Repo organisé** : GitFlow actif, branch protection, CODEOWNERS, PR template, commitlint, conventional commits
2. **CI verte** : build (matrix Ubuntu+Fedora × gcc+clang), lint, tests skeleton, doc build, commitlint
3. **Devcontainer + Docker** : tous les 6 peuvent build en local sans installer manuellement
4. **3 binaires "hello"** : `zappy_server`, `zappy_gui`, `zappy_ai` qui démarrent, parsent args (`--help`) et impriment "hello"
5. **Contrats protocole figés** : structs C++ + ADRs 001-008 Accepted
6. **Server core ↔ runtime split** : `libzappy_core` extrait, prêt pour pybind11
7. **Sample Vulkan triangle + cube** : la base GUI Vulkan compile et rend un triangle puis un cube texturé
8. **Sim Python stub** : `import zappy_sim` marche, expose une API stub testable
9. **Tests CI** : skeleton unit + integration + conformance, framework prêt
10. **Documentation MkDocs déployée** sur GitHub Pages (squelette)

## Goals NON dans S1 (volontairement reportés)

- Pas de logique de gameplay (vision cone, elevation, etc.)
- Pas de RL training réel
- Pas de scène 3D complète
- Pas de bonus (admin, prometheus, recorder)

---

## D1 — Lundi 25 mai

### 9h30-10h30 : Kickoff sprint planning
- Présentation du plan complet (PLAN.md)
- Lecture des rôles et CODEOWNERS
- Setup compte GitHub, accès SSH machine training
- Tour de table : qui a déjà fait quoi (Vulkan, RLlib, asio…)

### P1 — Léa (Server Lead)
- **Tâche** : Initialiser `server/CMakeLists.txt`, créer `core/` et `runtime/` skeleton
- Créer `server/runtime/main.cpp` qui parse `--help` (CLI11 stub), affiche `zappy_server v0.0.1\n`
- Créer `server/core/world_state.hpp` interface vide
- Créer header skeleton `server/core/{tile,player,egg,resource,event_scheduler,game_rules}.hpp`
- **Inputs** : nada
- **Outputs** : `zappy_server` binary compile + `--help` fonctionne
- **Critère** : `make` produit `zappy_server`, `./zappy_server --help` affiche usage
- **Risque** : conflits include / circular deps → utiliser pimpl ou forward declarations

### P2 — Marc (Server Net)
- **Tâche** : Setup `.github/` initial — CODEOWNERS, PR template, ISSUE templates (bug, feature, adr_request)
- Workflow `ci.yml` minimal : `commitlint` + `actions/checkout`
- Tester un commit avec mauvais format → CI rouge OK
- Tester un commit avec bon format → CI vert OK
- **Inputs** : nada
- **Outputs** : PR template visible, CODEOWNERS actif, commitlint en CI
- **Critère** : un faux PR avec commit non-conventional → bloqué par CI
- **Risque** : règles trop strictes au début, frustrent les devs → on relâche si vraiment besoin (mais on garde le format)

### P3 — Sami (GUI Lead)
- **Tâche** : Initialiser `gui/CMakeLists.txt`, deps vcpkg (`vulkan-headers`, `vk-bootstrap`, `vma`, `glfw3`, `glm`, `glslang`)
- Créer `gui/src/main.cpp` qui ouvre une fenêtre glfw vide noire
- Validation : Vulkan SDK 1.3 installé chez tout le monde
- Doc setup Vulkan SDK dans `docs/09_appendix/01_useful_commands.md`
- **Inputs** : nada
- **Outputs** : `zappy_gui` binary affiche fenêtre noire 800x600
- **Critère** : `./zappy_gui` ouvre fenêtre, ferme proprement avec ESC
- **Risque** : Vulkan SDK install différent Ubuntu/Fedora → P6 fournit dockerfile

### P4 — Inès (GUI Dev UX)
- **Tâche** : Setup `gui/include/zappy/gui/{ui,audio,input,net,scene}/` headers vides
- Ajouter Dear ImGui via vcpkg, créer un test "ImGui hello" dans une fenêtre glfw
- Repo asset : `gui/assets/README.md` avec règles license, organisation `models/`, `textures/`, `audio/`
- **Inputs** : `zappy_gui` skeleton de P3
- **Outputs** : ImGui démo standalone (binary test `gui_imgui_smoke`)
- **Critère** : ImGui windows ouvre dans le sample
- **Risque** : conflit init ImGui-Vulkan, à coordonner avec P3

### P5 — Théo (AI Lead RL)
- **Tâche** : Initialiser `ai_python/pyproject.toml` avec deps (torch, ray, pettingzoo, gymnasium, numpy, wandb)
- `ai_python/zappy_train/__init__.py` + structure modules
- Stub `zappy_train/env/pettingzoo_env.py` (juste les méthodes vides)
- Setup W&B (compte free, projet créé), Tensorboard
- **Inputs** : nada
- **Outputs** : `pip install -e ai_python` marche, `python -c "import zappy_train"` OK
- **Critère** : `pytest ai_python/tests/` passe (vide ou trivially)
- **Risque** : version conflit torch/ray → épingler tout

### P6 — Yanis (AI Dev / DevOps)
- **Tâche** : `docker/Dockerfile` base image Ubuntu 22.04 avec Vulkan SDK 1.3, gcc-13, clang-17, Python 3.11, CUDA 12.x runtime
- `docker/Dockerfile.training` extension avec PyTorch + ray + RLlib
- `docker/.devcontainer/devcontainer.json` pour VS Code
- `Makefile` root wrapper aux normes Epitech : `make`, `make re`, `make clean`, `make fclean`, `make tests_run`
- `CMakeLists.txt` root, options `ZAPPY_BUILD_TESTS`, `ZAPPY_BUILD_SIM`, `ZAPPY_HAS_RT`
- **Inputs** : nada
- **Outputs** : `docker build` OK, devcontainer ouvre dans VS Code, `make` racine compile
- **Critère** : un autre dev peut clone + `docker compose up devcontainer` + build le projet
- **Risque** : taille image, cache, ARG vs ENV — soigner

### Fin de journée D1
- Discord : chacun poste un screenshot de son binary "hello"
- Standup async sur la fin du jour si quelqu'un en retard

---

## D2 — Mardi 26 mai

### Standup 9h30

### P1
- **Tâche** : Définir `Tile`, `Player`, `Egg`, `Resource` structures C++ POD
- Implémenter `WorldState::WorldState(int w, int h)` + `at(x, y)` avec wrap toroidal
- Test `test_world_state_wrap.cpp` (1er Catch2 test du projet)
- Brouillon ADR-001 (server language C++17/20)
- **Outputs** : `WorldState` minimal compile + 1 test PASS
- **Critère** : `ctest` lance le test, vert

### P2
- **Tâche** : Workflow `ci.yml` matrix Ubuntu+Fedora × gcc+clang, build + ctest stubs
- Tester avec un PR triviale, vérifier 4 jobs verts
- ADR-019 (CI strategy)
- **Outputs** : CI matrix opérationnel
- **Critère** : 4 jobs CI passent sur un faux PR

### P3
- **Tâche** : Implémenter sample triangle Vulkan (suivant Sascha Willems `01_triangle`)
- Init via vk-bootstrap (instance, physical device, queue), swapchain, render pass dynamic
- Validation layers ON en debug
- ADR-002 (dynamic rendering vs render pass)
- **Outputs** : `./zappy_gui --sample triangle` affiche triangle coloré
- **Critère** : pas de validation layer warnings
- **Risque** : driver vulkan obsolète, on installe SDK 1.3.x partout

### P4
- **Tâche** : Sample ImGui-Vulkan intégré : ImGui DemoWindow par-dessus la fenêtre vide P3
- Setup input map avec glfw key callbacks (ESC, F3, Tab, F11 fullscreen)
- ADR-013 (ImGui docking layout)
- **Outputs** : ImGui DemoWindow s'affiche, F3 toggle visible
- **Critère** : ImGui interactif

### P5
- **Tâche** : Architecture `zappy_train/env/` : encode obs (placeholder shape), encode action space discret (30 actions)
- Stub `pettingzoo_env.py` `reset()` et `step()` qui retournent random tensors de bonne shape
- Test pytest `test_env_shapes.py`
- ADR-004 (RLlib vs SB3) + ADR-016 (curriculum design)
- **Outputs** : env stub fonctionne avec `rllib.rollout`
- **Critère** : `python -m zappy_train.training.train_ppo --debug --max-steps 100` ne crash pas

### P6
- **Tâche** : `sim_python/CMakeLists.txt` avec pybind11
- `sim_python/src/zappy_sim_pybind.cpp` : binding stub `zappy_sim.Sim` qui retourne random
- `tools/format_all.sh` + `tools/clang_tidy_runner.sh`
- `.pre-commit-config.yaml` complet + premier run sur tout le repo (`pre-commit run --all-files`)
- **Outputs** : `import zappy_sim; sim = zappy_sim.Sim()` marche
- **Critère** : pre-commit hooks fonctionnent localement

### Fin D2
- Demo dans `#standup` : screenshots P3 triangle + P4 ImGui + commit logs propres

---

## D3 — Mercredi 27 mai (M1 : contrats protocole figés)

### Standup 9h30

### Sync cross-team 10h-11h : protocole AI + GUI
**Participants** : P1, P2, P3, P4, P5, P6
**But** : figer les structs / messages utilisés
**Output** : `docs/01_architecture/06_protocols.md` validé par les 3 leads (P1, P3, P5) — **M1**

### P1
- **Tâche** : Définir `EventScheduler` interface, écrire 5 tests Catch2 ordering / cancellation
- Bouchonner `WorldState::move_forward/turn_left/right/take/set` (placeholders qui mutent l'état)
- ADR-005 (.zrec format) — coordination avec P2
- **Outputs** : `EventScheduler` interface + tests verts
- **Critère** : `ctest -L core` PASS

### P2
- **Tâche** : Spec complète protocole AI+GUI dans `docs/01_architecture/06_protocols.md`
- Implémenter `protocol_ai::parse` (split lignes, dispatch table commande → handler stub)
- Test `test_protocol_ai_parser.cpp`
- ADR-010 (Prometheus labels format)
- **Outputs** : parser PASS sur fixture cmds
- **Critère** : 100% commandes du sujet reconnues

### P3
- **Tâche** : Évoluer sample triangle → cube texturé (sample 04 Sascha Willems)
- VMA wrapper basique (`AllocatedBuffer`, `AllocatedImage`)
- Hot-reload shader glslang (recharge `.vert/.frag` si timestamp change)
- ADR-012 (shader hot-reload)
- **Outputs** : cube texturé tourne, hot-reload OK
- **Critère** : modifier shader source → recompile auto → cube change couleur

### P4
- **Tâche** : `gui/include/zappy/gui/net/gui_client.hpp` parser GUI protocol (line-based)
- Mock GUI client qui consomme un `.zrec` fixture
- ADR-014 (asset pipeline glTF + KTX2)
- **Outputs** : GUI client parse un dump, affiche dans console
- **Critère** : test `test_gui_protocol_parser.cpp` PASS

### P5
- **Tâche** : Encodage obs/action complet (selon `docs/01_architecture/04_ai_rl.md`)
- `reward.py` : reward shaping function avec unit tests
- ADR-017 (reward shaping)
- **Outputs** : pytest `test_reward.py` 100% PASS

### P6
- **Tâche** : Refactor `server/` en `server/core/` + `server/runtime/` (avec P1)
- `libzappy_core` cible CMake static lib + `zappy_server` exec link dessus
- ADR-003 (pybind11 strategy) + bouchon binding `zappy_sim` qui appelle vraiment `libzappy_core` (placeholder retour state vide)
- **Outputs** : `libzappy_core.a` + `zappy_sim.so` + import Python OK
- **Critère** : `python -c "import zappy_sim; s=zappy_sim.Sim(); print(s.reset(seed=42))"` retourne quelque chose

### Fin D3 — M1 atteint
- Push de tous les ADRs 001-008 mergés
- Posts Discord : "M1 done, contrats figés"

---

## D4 — Jeudi 28 mai

### Standup 9h30

### P1
- **Tâche** : Implémenter `WorldState::look(player_id)` complet avec vision cone + wrap toroidal
- Tests vision cone level 1..8 (8 tests Catch2)
- Documenter algorithme dans `docs/01_architecture/02_server.md`
- **Outputs** : `look()` testé exhaustivement
- **Critère** : tests vision toroidal PASS

### P2
- **Tâche** : NetworkLayer asio init + accept loop minimal (accepte connection, echo)
- Test integration `test_network_echo.cpp`
- ADR-009 (asio standalone)
- **Outputs** : `zappy_server` accepte connections sur port `-p`, echo OK
- **Critère** : `nc -v localhost 4242` reçoit echo

### P3
- **Tâche** : Frame graph squelette : abstraction `IPass`, `FrameGraph::execute()`
- Pass `GBufferPass` minimal : draw cube dans render target offscreen, compose à l'écran
- ADR-011 (frame graph design)
- **Outputs** : framegraph dessine cube via 2 passes
- **Critère** : RenderDoc capture shows 2 distinct passes

### P4
- **Tâche** : Caméra free orbit (mouse + WASD + zoom)
- Camera class avec matrices view/proj GLM
- Test `test_camera.cpp` : projection points connus
- **Outputs** : cube de P3 orbital avec souris
- **Critère** : peut tourner autour du cube, zoom in/out

### P5
- **Tâche** : RLlib config PPO multi-agent draft + script `train_ppo.py`
- Run training stub sur env stub : check pas de crash, samples_per_sec affiché
- Démo W&B tracking
- **Outputs** : run 60sec PPO sur env stub, W&B logs visibles
- **Critère** : W&B run dashboard accessible

### P6
- **Tâche** : `tests/conformance_sim_vs_runtime/` skeleton avec 1 scénario simple (move forward N fois, compare snapshots)
- Premier test conformance qui PASS sur le stub
- ADR-008 (broadcast codec) avec P2
- **Outputs** : conformance test infra prête
- **Critère** : `make conformance` PASS

### Fin D4
- Demo Friday + retro sont demain → finir features S1 ce soir si retard

---

## D5 — Vendredi 29 mai (M2 : socle livré)

### Standup 9h30

### P1
- **Tâche** : Finaliser tests `WorldState` (couverture 70%+ visée)
- Documenter API publique avec doxygen
- Buffer/rattrapage si retard sur D4
- **Outputs** : coverage rapport propre
- **Critère** : `gcovr server/core/` ≥ 70%

### P2
- **Tâche** : NetworkLayer poll loop + handshake AI (WELCOME → TEAM_NAME → CLIENT_NUM → X Y)
- Test `test_handshake.cpp`
- **Outputs** : `nc localhost 4242` reçoit WELCOME, on tape un team_name, on reçoit X Y
- **Critère** : handshake conforme protocole sujet

### P3
- **Tâche** : Bindless descriptor indexing setup (1 set géant pour textures)
- Sample : 2 cubes texturés différents avec bindless
- **Outputs** : bindless OK, doc dans `docs/01_architecture/03_gui_vulkan.md`
- **Critère** : RenderDoc → 1 seul descriptor set, 2 textures

### P4
- **Tâche** : HUD ImGui squelette : top bar "Status: connected", left panel teams (placeholder), bottom timeline (placeholder)
- Layout docking sauvé/chargé
- **Outputs** : HUD layout visible, dock OK
- **Critère** : F11 fullscreen, F3 toggle debug, Tab cycle camera mode (stub)

### P5
- **Tâche** : Eval pipeline stub : `python -m zappy_train.training.eval --opponent rule-based-stub` → win rate fake
- W&B + Streamlit dashboard ELO stub
- **Outputs** : pipeline eval fonctionnel sur stub
- **Critère** : dashboard accessible, rows insérables

### P6
- **Tâche** : MkDocs Material setup, `mkdocs serve` local OK, déploiement GitHub Pages
- Doxygen Doxyfile, intégration MkDocs
- Vérification CI nightly stub (workflow vide qui PASS)
- **Outputs** : site MkDocs déployé public sur `https://<org>.github.io/<repo>/`
- **Critère** : navigation 10+ pages OK

### 16h-17h : Demo Friday S1
- Chaque pôle montre :
  - **P1+P2** : `./zappy_server -p 4242 -x 10 -y 10 -n red -c 4 -f 100` + `nc localhost 4242` handshake live
  - **P3+P4** : `./zappy_gui` cube + ImGui + camera orbital
  - **P5+P6** : `python -m zappy_train.training.train_ppo --debug --max-steps 1000` + screenshot W&B
- Coverage report
- CI verte sur tous les jobs (capture)

### 17h-17h45 : Retro S1
- Stop / Start / Continue
- Mood check
- Actions sprint suivant

### Checkpoint M2 — Definition of Done sprint
- [ ] Repo GitFlow + branch protection + CODEOWNERS + PR template + commitlint **DONE**
- [ ] CI verte matrix Ubuntu+Fedora × gcc+clang **DONE**
- [ ] Devcontainer + Dockerfile + Makefile root **DONE**
- [ ] 3 binaires `--help` + structure CMake propre **DONE**
- [ ] Protocole AI + GUI documenté, structs C++ figées **DONE**
- [ ] Core/runtime split, `libzappy_core` lib statique + `zappy_sim.so` pybind11 **DONE**
- [ ] Vulkan triangle + cube texturé + bindless + hot-reload shaders **DONE**
- [ ] WorldState skeleton + look() complet + tests vision **DONE**
- [ ] AI env stub PettingZoo + reward shaping + RLlib PPO stub run **DONE**
- [ ] Conformance sim vs runtime infra prête **DONE**
- [ ] MkDocs + Doxygen déployés GitHub Pages **DONE**
- [ ] ADRs 001-008 (+ 009, 010, 011, 012, 013, 014, 016, 017, 019) Accepted **DONE**

Si tout ✅ → **M2 PASSÉ**, parallélisation S2 possible.
Si quelque chose ❌ → re-plan immédiat, P1+P3+P5+P6 sync samedi matin (exceptionnel).
