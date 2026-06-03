# 01 — Architecture overview

## Vue d'ensemble système

Le projet Zappy est composé de **4 composants déployables** :

```
                                    ┌──────────────────────────────┐
                                    │      zappy_server (C++17)    │
                                    │  - poll(2) socket multiplex  │
                                    │  - state machine du monde    │
                                    │  - time loop f-tick          │
                                    │  - admin/spectator socket    │
                                    │  - Prometheus exporter:9090  │
                                    │  - serializer .zrec optional │
                                    └──┬─────────────┬─────────────┘
                                       │ TCP         │ TCP
                                       │ AI proto    │ GUI proto
                                       │             │
        ┌──────────────────────────────┘             └─────────────────────────────┐
        │                                                                          │
┌───────▼───────────────────────────┐                          ┌────────────────────▼─────────┐
│      zappy_ai (C++ libtorch)      │                          │       zappy_gui (C++/Vulkan) │
│  - inference TorchScript model    │                          │  - Vulkan 1.3 + dyn rendering│
│  - rule-based fallback            │                          │  - bindless descriptor index │
│  - broadcast coded protocol       │                          │  - compute shader particles  │
│  - drives one drone               │                          │  - ray tracing (opt) reflets │
└───────────────────────────────────┘                          │  - ImGui HUD + debug F3      │
                                                                │  - 2D planisphère + 3D torus│
                                                                │  - miniaudio sfx + music    │
                                                                │  - replay reader .zrec      │
                                                                └──────────────────────────────┘

         ╔════════════════════════════════════════════════════╗
         ║         OFFLINE — Training pipeline                ║
         ╠════════════════════════════════════════════════════╣
         ║   libzappy_sim (C++ headless, no-socket)           ║
         ║                  ▲                                 ║
         ║                  │ pybind11                        ║
         ║                  │                                 ║
         ║   zappy_sim_env (Python, PettingZoo MultiAgent)    ║
         ║                  ▲                                 ║
         ║                  │                                 ║
         ║   RLlib PPO multi-agent + self-play tournament    ║
         ║                  │                                 ║
         ║                  ▼                                 ║
         ║   model.pt (TorchScript)  → packagé dans zappy_ai  ║
         ╚════════════════════════════════════════════════════╝
```

## Responsabilités par composant

### `zappy_server`
- **Source de vérité** unique : carte, joueurs, eggs, ressources, incantations en cours
- **Tick deterministe** : chaque action a un coût en `1/f` secondes, le serveur sérialise tout dans un event loop
- **Réseau** : single thread, `poll(2)` sur tous les sockets (clients AI + clients GUI + admin)
- **Broadcast events** : pousse les events GUI dès qu'ils se produisent (optimisé : ne pas re-broadcast tout l'état)
- **Persistence** : snapshot disque (JSON) + reload via signal `SIGUSR1`
- **Hot-reload config** : densité ressources, fréquence `f`, sans restart, via socket admin
- **Métriques** : exporte sur `/metrics` HTTP (port 9090) pour Prometheus

### `zappy_gui`
- **Client passif** : reçoit le flux protocole GUI du serveur, n'émet que `mct`, `bct`, `tna`, `ppo`, `plv`, `pin`, `sgt`, `sst`
- **Rendu** : 2D planisphère (vue rectangulaire) + 3D torus dans l'espace (caméra orbitale)
- **Bascule visu** via touche `Tab` : `2D / 3D / Split-screen 2D+3D / Top-down stratège`
- **HUD ImGui** : team scores, inventaire joueur sélectionné, timeline events, speed control
- **Debug F3** : graphes FPS, packets/sec, latence, mémoire, count d'objets, état GPU
- **Replay** : enregistre tous les packets serveur dans `.zrec`, peut les rejouer hors-ligne
- **Audio** : musique ambient + SFX (incantation, mort, broadcast, naissance)

### `zappy_ai`
- **Drives 1 drone** (1 process = 1 joueur, fork via commande `Fork` du serveur)
- **Inférence** : charge `model.pt` (TorchScript) au démarrage via libtorch, appelle `forward()` sur l'observation pour décider l'action
- **Fallback rule-based** : si `--no-model` ou modèle absent, utilise une FSM solide qui garantit l'évolution
- **Broadcast codé** : protocole interne team-only (header magic + intention encodée XOR avec team-key)
- **Resilience** : reconnect automatique si serveur down, buffer commandes (max 10 selon sujet)

### Training pipeline (offline)
- `libzappy_sim` : version statique de la logique serveur, sans réseau, exposée via `pybind11`
- `zappy_sim_env` : wrapper PettingZoo `ParallelEnv` (multi-agent), supporte des centaines d'env parallèles
- RLlib : PPO multi-agent avec policy partagée, self-play, curriculum sur taille de map et densité ennemie, ELO tracking
- Output : `model.pt` checkpoint TorchScript versionné dans `models/` (git-lfs)

## Stack résumé

| Composant | Langage | Build | Lib principales |
|-----------|---------|-------|-----------------|
| zappy_server | C++17/20 | CMake | asio, spdlog, nlohmann/json, CLI11, prometheus-cpp, Catch2 |
| zappy_gui | C++17/20 | CMake | Vulkan SDK 1.3, vk-bootstrap, VMA, glslang, GLM, glfw, Dear ImGui, miniaudio, libtorch |
| zappy_ai | C++17/20 | CMake | libtorch, asio, nlohmann/json |
| libzappy_sim | C++17/20 | CMake | (sous-set zappy_server, no asio, no spdlog) + pybind11 |
| training | Python 3.11 | pip + pyproject | torch, ray[rllib], pettingzoo, numpy, wandb, tensorboard |

## Décisions architecturales structurantes

Voir [`docs/03_process/04_adrs.md`](../03_process/04_adrs.md) pour le workflow ADR.

ADRs prévues dès S1 :
- ADR-001 : Choix C++17/20 pour le serveur (vs C, vs Rust)
- ADR-002 : Vulkan 1.3 dynamic rendering vs render pass legacy
- ADR-003 : pybind11 pour exposer `libzappy_sim` au training Python
- ADR-004 : RLlib multi-agent vs SB3 single-agent
- ADR-005 : Format `.zrec` binaire custom vs JSON Lines
- ADR-006 : libtorch C++ inférence vs ONNX Runtime
- ADR-007 : Stratégie de fallback ray tracing pour non-RTX
- ADR-008 : Broadcast codé entre AIs de même team
