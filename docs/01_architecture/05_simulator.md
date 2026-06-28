# 05 — Simulateur turbo pour training

## Objectif

Pour entraîner un agent RL multi-agent, on a besoin de **millions d'étapes/seconde**. Lancer `zappy_server` réel + clients réseau pour chaque step est beaucoup trop lent. La solution :

> **Réutiliser la logique du vrai serveur**, mais sans la couche réseau ni le sleep du time loop → l'expose en lib statique → l'invoque depuis Python via `pybind11`.

## Avantages clés

- **Zéro divergence** entre training et runtime : c'est le MÊME code C++ que le serveur prod, ce qui élimine les surprises sim2real.
- **Performances natives** : pas d'overhead Python pour la logique de game.
- **Parallélisation triviale** : N environnements en parallèle dans des threads workers RLlib.

## Architecture

```
            ┌────────────────────────────────────────────┐
            │                Server core lib             │
            │  WorldState, EventScheduler, GameRules     │
            │  (pas de réseau, pas de spdlog, pas        │
            │   de prometheus, pas de signal handling)   │
            └─────────────┬──────────────────────────────┘
                          │
                          │ #include
                          │
            ┌─────────────▼──────────────┐
            │     libzappy_sim.a          │   ← cible CMake `zappy_sim_core`
            │  - reset(seed, config)      │
            │  - step(actions[]) → obs[], │
            │    rewards[], dones[]       │
            │  - get_state_snapshot()     │
            │  - get_visible_for(player)  │
            └─────────────┬──────────────┘
                          │
                          │ pybind11 binding
                          │
            ┌─────────────▼──────────────┐
            │     zappy_sim (.so)         │   ← cible CMake `zappy_sim_pybind`
            │  Python module              │
            └─────────────┬──────────────┘
                          │
                          │ import zappy_sim
                          │
            ┌─────────────▼──────────────┐
            │  zappy_train.env             │
            │  PettingZoo ParallelEnv      │
            │  - obs encoding              │
            │  - reward shaping            │
            │  - episode termination       │
            └─────────────┬──────────────┘
                          │
                          │ rllib
                          │
            ┌─────────────▼──────────────┐
            │  Ray + RLlib PPO            │
            │  - N workers parallèles      │
            │  - chacun avec K envs        │
            │  - policy partagée           │
            └─────────────────────────────┘
```

## Refactoring requis côté serveur (S1 - S2)

Pour permettre cette extraction, le `zappy_server` doit être organisé en **core + adapters** :

```
server/
├── core/           ← lib portable, indépendante de asio/spdlog/réseau
│   ├── world_state.{hpp,cpp}
│   ├── event_scheduler.{hpp,cpp}
│   ├── game_rules.{hpp,cpp}    ← elevation rules, vision cone, broadcast directionality
│   └── ...
└── runtime/        ← le vrai serveur, link au core + réseau/log/metrics
    ├── network_layer.{hpp,cpp}
    ├── protocol_ai.{hpp,cpp}
    ├── ...
    └── main.cpp
```

CMake :
```cmake
add_library(zappy_core STATIC core/...)        # no deps réseau
target_link_libraries(zappy_core PUBLIC ${threads})

add_executable(zappy_server runtime/main.cpp ...)
target_link_libraries(zappy_server PRIVATE zappy_core asio spdlog ...)
```

Cette séparation est une **prérequis dure** : sans ça, pas de simulateur. C'est pour ça qu'elle est dans le sprint **S1** (foundations).

## API publique de `libzappy_sim`

```cpp
namespace zappy::sim {

struct SimConfig {
    int width = 10;
    int height = 10;
    std::vector<std::string> teams;
    int max_clients_per_team = 4;
    int max_steps = 100'000;
    uint64_t seed = 0;
    bool enable_eggs = true;
};

struct AgentObservation {
    std::array<int8_t, 256> vision_flat;  // cone vision encoded
    std::array<float, 8>    inventory;
    int                     level;
    int                     orientation;  // 0..3
    float                   time_in_game_norm;
    int                     team_id;
    std::array<int8_t, 32>  last_broadcast;
};

struct AgentAction {
    int action_id;  // 0..N-1 ; voir ai_rl.md
};

struct StepResult {
    std::vector<AgentObservation> observations;
    std::vector<float> rewards;
    std::vector<bool> dones;
    bool game_over;
};

class Sim {
public:
    explicit Sim(SimConfig cfg);
    StepResult reset(uint64_t seed);
    StepResult step(const std::vector<AgentAction>& actions);
    nlohmann::json get_state_snapshot() const;
    int n_agents() const;
};

}
```

## Binding pybind11 (extrait)

```cpp
PYBIND11_MODULE(zappy_sim, m) {
    py::class_<SimConfig>(m, "SimConfig")
        .def(py::init<>())
        .def_readwrite("width", &SimConfig::width)
        .def_readwrite("height", &SimConfig::height)
        // ...
        ;
    py::class_<Sim>(m, "Sim")
        .def(py::init<SimConfig>())
        .def("reset", &Sim::reset)
        .def("step", &Sim::step)
        .def("n_agents", &Sim::n_agents)
        ;
}
```

## Performance cibles

| Métrique | Cible |
|----------|-------|
| Steps/sec single env (map 20x20, 4 players) | ≥ 1 000 000 |
| Steps/sec single env (map 50x50, 24 players) | ≥ 200 000 |
| Mémoire RSS par env (map 50x50) | < 5 MB |
| Throughput aggregate sur machine 16 cores | ≥ 100 000 samples/sec (avec overhead RLlib) |

## Tests

- **Conformity test** (CRITIQUE) : pour un set de scénarios YAML, comparer **bit-à-bit** l'évolution du `WorldState` produit par `libzappy_sim` versus le `zappy_server` runtime. Lancé à chaque PR.
- **Determinism test** : pour `seed` fixé, `reset → step×N` produit toujours le même `StepResult`.
- **Performance regression** : benchmark CI nightly, échec si steps/sec drop > 10%.

## Limites / non-buts

- Pas de support multi-thread interne (1 sim = 1 thread). La parallélisation = N sims = N workers RLlib.
- Pas de support broadcast inter-process (les broadcasts sont locaux à la sim).
- Pas d'admin/spectator dans la sim (inutile).
- Pas de recorder `.zrec` (overhead inutile).

## Conformance avec le sujet

Le sujet impose `single process / single thread` pour le serveur en mode normal. Le simulateur est un binaire séparé (`libzappy_sim`) destiné UNIQUEMENT à l'offline training. **Le binaire `zappy_server` livré reste conforme**.
