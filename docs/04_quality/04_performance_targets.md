# 04 — Cibles de performance

## Cibles fixées (validées round 5)

| Composant | Métrique | Cible | Conditions de mesure |
|-----------|----------|-------|----------------------|
| GUI | FPS médian | **≥ 60** | 4K (3840x2160), map 200x200, 4 teams × 6 players, scène cinématique 30s |
| GUI | FPS p99 | ≥ 45 | même conditions |
| GUI | Frametime stable | < 5% jitter | sur 1000 frames consécutives |
| GUI | GPU memory | < 4 GB | même scène |
| Server | Ticks/sec | **≥ 500** | map 200x200, 24 players actifs, f=500 |
| Server | Mémoire RSS | < 300 MB | même conditions |
| Server | Latence action → ack | < 2 ms | hors temps cost de l'action |
| Sim | Steps/sec single env | ≥ 1 000 000 | map 20x20, 4 players |
| Sim | Steps/sec single env | ≥ 200 000 | map 50x50, 24 players |
| Sim | Steps/sec aggregate | ≥ 100 000 | RLlib 16 workers × 8 envs/worker (au choix) |
| Training | Throughput samples/sec | **≥ 100 000** | machine 64GB + RTX, PPO multi-agent |
| Inference C++ | Latence forward libtorch | < 2 ms | single agent, batch size 1 |
| Replay | Lecture .zrec → tick GUI | < 1 ms overhead vs live | partie de 5 min |

## Mesure / monitoring

### GUI
- GPU timestamps via `vkCmdWriteTimestamp` autour de chaque pass
- Frametime via `std::chrono::steady_clock`
- Affichage debug F3 : graphe sur 240 dernières frames
- VMA stats : `vmaGetStatistics()` exporté en debug panel
- RenderDoc capture nightly pour analyse profonde

### Server
- Prometheus exporter sur `/metrics:9090` :
  - `zappy_tick_duration_seconds` histogram
  - `zappy_actions_per_tick` gauge
  - `zappy_clients_count` gauge
- Dashboard Grafana `grafana/zappy_overview.json`
- Profil : `perf record` sur 60s pour profiling cold path

### Sim
- Google Benchmark intégré CMake
- `tools/bench_sim.sh` : N x 1M steps, mean ± std
- Sortie JSON : `{ "steps_per_sec_mean": 1234567, "memory_mb_max": 4.2 }`

### Training
- W&B logs : `samples_per_sec`, `train/loss`, `eval/win_rate_vs_ref`
- TensorBoard local pour réplay
- Tracé ELO sur dashboard streamlit

### Inference libtorch
- Bench : `tools/bench_libtorch_inference.cpp` : N forward, mean latency, p99 latency

## Profilers utilisés

| Outil | Usage |
|-------|-------|
| `perf` | CPU profiling Linux, hot functions server / sim |
| `Tracy` | Real-time profiler instrumenté, GUI et server |
| `RenderDoc` | Vulkan frame analysis, état GPU détaillé |
| `Nsight Graphics` | Profiling GPU NVIDIA (si dispo) |
| `valgrind --tool=massif` | Memory profiling (rare, on-demand) |
| `cargo-flamegraph` / `flamegraph.pl` | Visualisation profile |
| Tensorboard / W&B | Training profiling |

## Optimisations prioritaires

### Server
1. `WorldState::look()` : précomputer le pattern de vision par level pour éviter recalculs
2. `EventScheduler` : `boost::heap::d_ary_heap` plus rapide que `std::priority_queue` si profilage le justifie
3. Bytes serialization protocole GUI : buffer pré-alloué, `fmt::format_to` plutôt que `std::format`+`+=`
4. Pas d'allocations dans le hot path tick → réserver les vectors

### GUI
1. Bindless descriptor → 1 single bind point pour toutes textures, plus de rebind
2. Dynamic rendering → moins de driver overhead que render pass
3. Compute particles → GPU only, pas de CPU→GPU upload par frame
4. Cull torus pieces hors frustum
5. LOD trantorian mesh (3 niveaux : near, mid, far)
6. TAA pour réduire la charge SSAO sample count
7. Pre-rotated swapchain (Android-style) si applicable

### Sim
1. `WorldState` POD struct, `std::vector` pré-alloué
2. `Sim::step` zéro allocation (buffers pré-alloués pour obs, rewards)
3. Vision cone précalculé via lookup table
4. SIMD pour batch obs encoding (optionnel S4)

### Training
1. RLlib `num_envs_per_worker` tuné pour saturer GPU
2. `torch.compile()` la policy (PyTorch 2.x)
3. Mixed precision FP16 si convergence OK
4. Replay buffer pré-alloué

## Anti-patterns à fuir

- ❌ `std::endl` (flush systématique, lent)
- ❌ `std::string` concat dans une loop chaude
- ❌ Allocation dans une boucle hot
- ❌ `std::shared_ptr` quand `unique_ptr` ou stack suffit
- ❌ Lambda capture par valeur d'un gros objet
- ❌ Python loop sur 1M items quand vectorisable numpy

## Benchmarks dans la CI

- `nightly.yml` job `benchmarks` :
  - lance tous les bench
  - écrit `bench_YYYYMMDD.json`
  - compare à la moyenne mobile 7 jours
  - **fail** si régression > 10%
  - poste sur Discord `#ci-alerts` 📉 si fail

## Tracking historique

Tous les `bench_*.json` archivés dans GitHub Artifacts (90j retention).
Dashboard Grafana branché sur Prometheus pushgateway pour tracking long terme.

## Validation finale (S4)

Avant la soutenance (J+25), un **bench manuel** complet est lancé sur la machine de démo :
- 60 FPS @ 4K confirmé
- 500 ticks/sec confirmé
- 100k steps/sec confirmé
- Latence inférence < 2 ms confirmée
- Démarrage à froid GUI < 3 sec
- Démarrage à froid serveur < 1 sec

Si une cible KO : intervention dernière minute ou downgrade visible documenté dans les slides (transparence).
