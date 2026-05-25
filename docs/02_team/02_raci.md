# 02 — Matrice RACI

**Légende** :
- **R** = Responsible (fait le travail)
- **A** = Accountable (rend des comptes, owner final)
- **C** = Consulted (avis sollicité avant décision)
- **I** = Informed (informé du résultat)

| Tâche / Domaine | P1 Léa<br>Server Lead | P2 Marc<br>Server Net | P3 Sami<br>GUI Vulkan | P4 Inès<br>GUI UX | P5 Théo<br>AI RL | P6 Yanis<br>AI/Sim/DevOps |
|---|:--:|:--:|:--:|:--:|:--:|:--:|
| **Setup repo / GitFlow / branch protection** | C | C | C | I | I | R/A |
| **Devcontainer + Docker images** | I | C | C | I | C | R/A |
| **CI workflows (build, lint, test, bench)** | C | C | C | I | C | R/A |
| **Pre-commit hooks** | C | C | C | I | C | R/A |
| **CMake root + structure cmake/** | C | I | C | I | I | R/A |
| **Refactor server core ↔ runtime split** | R/A | C | I | I | C | C |
| **WorldState + Tile + Player** | R/A | C | I | I | C | C |
| **EventScheduler + tick loop** | R/A | C | I | I | I | C |
| **Game rules (elevation, vision, broadcast)** | R/A | C | I | I | C | C |
| **NetworkLayer poll/asio** | C | R/A | I | I | I | I |
| **Protocol AI parser/dispatcher** | C | R/A | I | I | C | I |
| **Protocol GUI emitter** | C | R/A | C | C | I | I |
| **Protocol admin (bonus)** | C | R/A | I | I | I | I |
| **Recorder .zrec** | I | R/A | I | C | I | I |
| **Metrics Prometheus + Grafana** | I | R/A | I | I | I | C |
| **Persistance snapshot + hot-reload config** | R/A | C | I | I | I | I |
| **Tests server unit (Catch2)** | R/A | R | I | I | I | I |
| **Vulkan instance/device/swapchain** | I | I | R/A | C | I | I |
| **VMA memory wrapper** | I | I | R/A | I | I | I |
| **Frame graph** | I | I | R/A | C | I | I |
| **Pass : shadow, gbuffer, lighting** | I | I | R/A | C | I | I |
| **Pass : ssao, taa, bloom, tonemap** | I | I | R/A | C | I | I |
| **Pass : atmosphere scattering** | I | I | R/A | C | I | I |
| **Pass : RT reflection (opt)** | I | I | R/A | I | I | I |
| **Compute particles** | I | I | R/A | C | I | I |
| **Hot-reload shaders glslang** | I | I | R/A | I | I | I |
| **Bindless descriptor indexing** | I | I | R/A | I | I | I |
| **Torus mesh + skybox + procedural** | I | I | C | R/A | I | I |
| **Trantorian / resource models** | I | I | C | R/A | I | I |
| **Camera modes (free/follow/topdown)** | I | I | C | R/A | I | I |
| **HUD ImGui complet** | I | I | C | R/A | I | I |
| **Debug panel F3** | I | I | C | R/A | I | I |
| **Speed control + step-by-step** | I | C | C | R/A | I | I |
| **Replay reader .zrec** | I | C | C | R/A | I | I |
| **Audio engine (miniaudio)** | I | I | C | R/A | I | I |
| **Input map glfw** | I | I | C | R/A | I | I |
| **GUI net client (parse protocol)** | I | C | C | R/A | I | I |
| **Asset pipeline (gltf, ktx2)** | I | I | C | R/A | I | C |
| **Refactoring libzappy_sim** | C | I | I | I | C | R/A |
| **pybind11 bindings** | C | I | I | I | C | R/A |
| **PettingZoo env wrapper** | I | I | I | I | R/A | C |
| **Observation / action / reward encoding** | I | I | I | I | R/A | C |
| **RLlib PPO config + train script** | I | I | I | I | R/A | C |
| **Curriculum learning** | I | I | I | I | R/A | C |
| **Self-play + ELO** | I | I | I | I | R/A | I |
| **Eval pipeline (vs zappy_ref)** | C | C | I | I | R/A | C |
| **Export TorchScript .pt** | I | I | I | I | R/A | C |
| **W&B + Streamlit dashboards** | I | I | I | I | R/A | I |
| **zappy_ai binary C++ libtorch** | I | I | C | I | C | R/A |
| **Rule-based FSM fallback** | C | I | I | I | C | R/A |
| **Broadcast codec team** | C | C | I | I | C | R/A |
| **Tests python (pytest + mutmut)** | I | I | I | I | R/A | R |
| **Tests integration AI↔server YAML** | R | R | I | I | R/A | R |
| **Tests conformance sim vs runtime** | R | I | I | I | C | R/A |
| **Benchmarks (FPS, ticks/s, samples/s)** | C | C | C | C | C | R/A |
| **ADRs (review + merge)** | A | A | A | A | A | A |
| **MkDocs site + GitHub Pages** | C | C | C | C | C | R/A |
| **Doxygen API docs** | R | R | R | R | R | A |
| **Vidéo démo 5 min** | C | C | R | R/A | C | C |
| **Slides soutenance** | C | C | C | C | C | R/A |
| **Démo live soutenance** | R | R | R | R | R | R/A |
| **On-call CI / red CI** | I | I | I | I | I | R/A |
| **Release tagging + changelog** | I | I | I | I | I | R/A |

## Règle d'arbitrage

- **Conflit intra-pôle** : le Lead du pôle tranche (P1 server, P3 GUI, P5 AI)
- **Conflit cross-pôle sur contrat** (protocole, structures partagées) : décision collégiale des 3 Leads + ADR. Si pas de consensus en 24h, vote des 6.
- **Conflit infra/CI/DevOps** : P6 décide (Accountable)
- **Conflit release/soutenance** : décision collégiale, P6 facilitateur logistique
