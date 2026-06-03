# 00 — Résumé exécutif

## 1. Objectif du projet

Réaliser **à 100%** le projet Epitech G-YEP-400 **Zappy** : un jeu réseau multi-équipes sur monde torus, comprenant 3 binaires :
- `zappy_server` : serveur autoritatif (C++17/20, single-thread, poll)
- `zappy_gui` : client graphique (C++ + Vulkan 1.3, rendu 2D planisphère + 3D torus dans l'espace)
- `zappy_ai` : client IA (Python pour le training PyTorch + RLlib, livrable en C++ libtorch pour l'inférence)

**Critère de victoire interne** : 6 personnes capables de présenter, le vendredi 19 juin 2026, une démo live qui :
- joue une partie complète sans crash,
- montre la planète torus 3D dans l'espace avec atmosphère scattering, étoiles procédurales, ray tracing pour reflets,
- montre une IA RL entraînée qui bat l'IA du serveur de référence (`zappy_ref-v3.0.1.tgz`) sur >=70% des matches,
- montre les bonus : replay (.zrec), HUD complet, debug panel F3, cam libre/follow/topdown, speed control 0.25x..16x, admin/spectator, Prometheus/Grafana,
- a une CI verte, une couverture de tests >=70% sur le core C++, et une documentation complète en MkDocs.

## 2. Stack technique figée

### Serveur (C++17/20)
- `asio` standalone (header-only) au-dessus de `poll(2)` (compatible exigence sujet)
- `spdlog` pour logging structuré
- `nlohmann/json` pour config + état serialization
- `CLI11` pour parsing arguments
- `Catch2` v3 pour tests unitaires
- `prometheus-cpp` pour métriques

### GUI (C++17/20)
- **Vulkan 1.3** avec dynamic rendering (zéro render pass legacy)
- `vk-bootstrap` pour init/device/queues
- `VulkanMemoryAllocator` (VMA) pour memory management
- `glslang` pour compilation shaders runtime (HMR shaders)
- `GLM` pour math
- `glfw` pour fenêtre + input
- `Dear ImGui` pour HUD et debug panel
- `miniaudio` (header-only) pour audio
- `libtorch` C++ (optionnel intégré pour preview model)
- Features Vulkan ciblées : dynamic_rendering, descriptor_indexing (bindless), compute_shader, ray_tracing_pipeline (avec fallback)
- Shaders custom : skybox étoiles + nebula (FBM noise), atmosphere scattering Rayleigh/Mie, water/lava biomes, post-FX (Bloom + TAA + SSAO + tonemap ACES)

### AI (Python 3.11 + libtorch C++)
- **Training** : Python 3.11, PyTorch 2.x, PettingZoo (multi-agent env), RLlib (PPO + multi-agent self-play)
- **Inférence** : modèle exporté en `torch.jit` (TorchScript) → chargé en C++ via `libtorch` dans `zappy_ai` binary
- Simulateur turbo : le serveur C++ compilé avec flag `-DZAPPY_SIMULATION_MODE` désactive sockets, expose une lib `libzappy_sim` consommée via `pybind11`
- Wrapper Gym/PettingZoo Python par-dessus pour RLlib

### DevOps / qualité
- **Plateforme** : Linux Ubuntu 22.04 et Fedora 39+ seulement
- **Build** : CMake 3.25+ principal, Makefile root wrapper aux normes Epitech (`make`, `make re`, `make clean`, `make fclean`, `make tests_run`)
- **Container** : Dockerfile + devcontainer VS Code (Vulkan SDK 1.3.x, gcc-13, clang-17, Python 3.11, PyTorch, CUDA 12.x runtime)
- **Git** : GitFlow strict (main, develop, feature/*, release/*, hotfix/*)
- **CI** : GitHub Actions (build matrix Ubuntu+Fedora, lint, tests unit/integration, bench nightly, deploy docs MkDocs)
- **Quality gates** : clang-format + clang-tidy + cppcheck + ruff + mypy strict + black, en pre-commit ET en CI bloquant
- **PR** : template obligatoire, conventional commits + commitlint, CODEOWNERS, 2 reviews requis
- **Docs** : MkDocs Material + mkdocs-material plugin + Doxygen pour API C++, déployé GitHub Pages, en **français**
- **Tests** : Catch2 (unit C++), pytest+mutmut (Python), suite d'intégration AI↔server YAML scenarios, benchmarks nightly
- **Replay** : format binaire custom `.zrec` (magic header + frames timestampées)
- **License** : privé/propriétaire (pas de fichier LICENSE)

## 3. Équipe & rôles (6 personnes)

| ID | Rôle | Domaine | Calendrier perso |
|----|------|---------|------------------|
| P1 | Server Lead | Game logic, état du monde, time-loop, persistance, hot-reload config | [07_calendar_per_person/P1](07_calendar_per_person/P1_server_lead.md) |
| P2 | Server Dev | Réseau (poll/asio), protocole AI+GUI, admin/spectator, Prometheus | [P2](07_calendar_per_person/P2_server_dev.md) |
| P3 | GUI Lead | Renderer Vulkan, frame graph, dynamic rendering, bindless, ray tracing, post-FX | [P3](07_calendar_per_person/P3_gui_lead_vulkan.md) |
| P4 | GUI Dev | Scène 2D planisphère + 3D torus, HUD ImGui, replay, cameras, audio | [P4](07_calendar_per_person/P4_gui_dev_ux.md) |
| P5 | AI Lead | Env multi-agent PettingZoo, RLlib pipeline, reward design, curriculum, ELO | [P5](07_calendar_per_person/P5_ai_lead_rl.md) |
| P6 | AI Dev | Simulateur turbo (pybind11), libtorch inference C++, DevOps/CI/Docker | [P6](07_calendar_per_person/P6_ai_dev_sim_devops.md) |

Détail des responsabilités : [02_team/01_roles.md](02_team/01_roles.md). Matrice RACI : [02_team/02_raci.md](02_team/02_raci.md).

## 4. KPIs de succès (mesurables)

| Catégorie | KPI | Cible |
|-----------|-----|-------|
| Fonctionnel | Compatibilité protocole avec serveur de référence | 100% commands AI + 100% events GUI |
| Fonctionnel | Tests d'intégration AI↔server passent | 100% des scenarios YAML |
| Performance | FPS GUI @ 4K, map 200x200, 4 teams x 6 players | ≥ 60 FPS (médiane) |
| Performance | Server ticks/sec, map 200x200, 24 players | ≥ 500 |
| Performance | Training samples/sec sur machine 64GB+RTX | ≥ 100k |
| Qualité | Couverture tests unit C++ core | ≥ 70% |
| Qualité | Couverture tests Python (zappy_ai) | ≥ 80% |
| Qualité | CI build + tests | 100% des PR mergées avec CI verte |
| IA | Win rate contre `zappy_ref` (100 matches) | ≥ 70% |
| Docs | ADRs pour chaque décision archi | 100% |
| Docs | Site MkDocs déployé | OUI à J+25 |

## 5. Risques principaux (top 3)

1. **Ray tracing Vulkan** : peu de membres ont RTX (2-3 sur 6) → développement bloqué par accès matériel. **Mitigation** : feature flag `--enable-rt`, pipeline fallback rasterized obligatoire dès S2.
2. **Convergence RL** : RLlib multi-agent peut ne pas converger en 4 semaines. **Mitigation** : IA rule-based v1 livrée S2 comme baseline + fallback; RL = bonus.
3. **Glue Vulkan** : courbe d'apprentissage Vulkan 1.3 + RT + bindless + compute. **Mitigation** : 2 devs full-time GUI dont 1 lead expérimenté; vk-bootstrap pour abstraction init; jour 1 dédié au sample triangle Vulkan; Sascha Willems samples utilisés comme référence.

Voir le détail : [05_risks/01_top_10_risks.md](05_risks/01_top_10_risks.md).

## 6. Outils de collaboration

- **Code** : GitHub (repo `G-YEP-400-RUN-4-1-zappy-3`)
- **Chat** : Discord (channels `#general`, `#server`, `#gui`, `#ai`, `#ci-alerts`, `#standup`)
- **Board** : GitHub Projects (Kanban) avec colonnes `Backlog / Todo Sprint / In Progress / Review / Done`
- **Docs** : MkDocs Material publié sur GitHub Pages
- **Réunions** :
  - Standup 9h30 (15 min, async possible sur Discord si retard)
  - Retro vendredi 17h (45 min)
  - Planning lundi 9h30 (1h)
- **Code review SLA** : <=4h en heures ouvrées, <=24h sinon

## 7. Calendrier global

Voir [06_calendar/00_overview.md](06_calendar/00_overview.md) pour le Gantt et la justification de la cadence sprint.

**Quatre sprints d'une semaine** :
- S1 (25-29 mai) — **Foundations** : socle commun, CI, contrats. Aucune feature avant la fin du sprint.
- S2 (1-5 juin) — **Core** : MVP de chaque pôle. Fin de S2 = jouer une partie complète AI rule-based + GUI base + server v1.
- S3 (8-12 juin) — **Bonus + intégration** : training RL, shaders custom, broadcast codé, admin/spectator.
- S4 (15-19 juin) — **Polish + démo** : RT, post-FX final, freeze code, soutenance vendredi.

## 8. Definition of "100% du projet réalisé"

À J+25, on considère le projet "à 100%" si **TOUTES** les conditions sont remplies :

- [ ] `make` à la racine produit `zappy_server`, `zappy_gui`, `zappy_ai` sans warning
- [ ] Tous les tests CI passent (unit + integration + benchmarks dans les cibles)
- [ ] Une partie standard `./zappy_server -p 4242 -x 50 -y 50 -n red blue -c 6 -f 100` + `./zappy_gui -p 4242 -h localhost` + 4×`./zappy_ai -p 4242 -n red -h localhost` se déroule sans crash jusqu'à game over
- [ ] Le rendu GUI 3D affiche la planète torus dans l'espace, étoiles, atmosphère, water/lava
- [ ] Le HUD affiche : team scores, inventaire joueur sélectionné, timeline événements, speed control
- [ ] Le replay `.zrec` peut être enregistré pendant une partie et rejoué hors-ligne
- [ ] Mode admin/spectator opérationnel
- [ ] Métriques Prometheus exposées par le serveur, dashboard Grafana fourni
- [ ] L'IA RL gagne >=70% des matches contre `zappy_ref`
- [ ] Documentation MkDocs déployée sur GitHub Pages, en français
- [ ] Vidéo démo 5 min produite + slides soutenance prêts
- [ ] ADRs présents pour toutes les décisions structurantes
