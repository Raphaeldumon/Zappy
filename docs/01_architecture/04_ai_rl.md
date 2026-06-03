# 04 — Architecture IA + RL

## Périmètre

- **Binaire `zappy_ai`** : 1 process = 1 drone. C++ libtorch pour l'inférence du modèle entraîné.
- **Fallback rule-based** : FSM solide garantissant survie + élévation, livrée S2 — utilisée si pas de modèle, ou si le modèle plante / sous-performe.
- **Broadcast codé** : protocole intra-team (header magic + payload XOR avec team-key).
- **Training** : pipeline Python (PyTorch + RLlib + PettingZoo) qui consomme le simulateur C++ via pybind11.
- **Self-play tournament + ELO** pour évaluer les checkpoints.
- **Curriculum learning** : map size + ennemis croissants.

## Composants

### `zappy_ai` (C++ binaire)

```
ai_cpp/
├── CMakeLists.txt
├── include/zappy/ai/
│   ├── agent.hpp
│   ├── observation.hpp        ← struct + sérialisation tensor libtorch
│   ├── action.hpp
│   ├── policy.hpp             ← interface (Inference || RuleBased)
│   ├── policy_inference.hpp   ← libtorch loader + forward
│   ├── policy_rule_based.hpp  ← FSM
│   ├── client.hpp             ← protocole AI↔server
│   ├── broadcast_codec.hpp    ← encode/decode messages team
│   └── memory.hpp             ← buffer historique pour LSTM (si applicable)
├── src/...
└── tests/
```

### `libzappy_sim` + `pybind11` bindings

```
sim_python/
├── CMakeLists.txt              ← pybind11 module
├── src/zappy_sim_pybind.cpp
└── tests/
```

Expose en Python :
```python
import zappy_sim
env = zappy_sim.Sim(width=20, height=20, n_teams=2, max_players=6, f=1000)
obs = env.reset(seed=42)
obs, rewards, dones, infos = env.step(actions)
```

### `zappy_train` (Python)

```
ai_python/
├── pyproject.toml
├── zappy_train/
│   ├── __init__.py
│   ├── env/
│   │   ├── pettingzoo_env.py     ← wrapper PettingZoo ParallelEnv
│   │   ├── observation.py        ← encodage obs (vision, inventory, level, broadcasts)
│   │   ├── action.py             ← discrete action space
│   │   └── reward.py             ← reward shaping multi-obj
│   ├── policies/
│   │   ├── ppo_policy.py
│   │   └── attention_policy.py   ← optional transformer policy
│   ├── training/
│   │   ├── train_ppo.py          ← RLlib launcher
│   │   ├── self_play.py          ← tournament + ELO
│   │   ├── curriculum.py
│   │   └── eval.py               ← run vs ref-server, count win
│   ├── export/
│   │   └── export_torchscript.py ← .pt pour libtorch
│   ├── viz/
│   │   ├── replay_viewer.py      ← matplotlib pour debug
│   │   └── elo_dashboard.py      ← streamlit/dash
│   └── config/
│       ├── ppo_default.yaml
│       └── curriculum_default.yaml
├── tests/
└── notebooks/
```

## Observation space (multi-agent par drone)

Chaque drone reçoit :

| Field | Shape | Description |
|-------|-------|-------------|
| `vision_flat` | (256,) | Vision cone aplatie + padded, encoded {one-hot resource + player presence} |
| `inventory` | (8,) | food, linemate, deraumere, sibur, mendiane, phiras, thystame, food_remaining_normalized |
| `level` | (8,) | one-hot level 1..8 |
| `orientation` | (4,) | one-hot N/E/S/W |
| `time_in_game_norm` | (1,) | t/T_max |
| `team_id` | (1,) | normalized |
| `last_broadcast` | (32,) | last received broadcast direction + encoded payload |
| `team_global_stats` | (16,) | nb players alive, nb players at lvl 8, nb eggs, nb pending incantations |
| `recent_actions_hist` | (16,) | one-hot des 16 dernières actions de cet agent |

Total dim : ~342 floats per agent.

## Action space (discret)

```python
ACTIONS = [
    'Forward', 'Right', 'Left',           # mouvement
    'Look',                               # observe
    'Inventory',                          # synchro inventory
    'Broadcast:HELP', 'Broadcast:HERE',   # broadcasts encodés (codebook fixe court)
    'Broadcast:READY_LVL', 'Broadcast:GATHER',
    'Connect_nbr',
    'Fork',
    'Eject',
    'Take:food', 'Take:linemate', 'Take:deraumere',
    'Take:sibur', 'Take:mendiane', 'Take:phiras', 'Take:thystame',
    'Set:food', 'Set:linemate', 'Set:deraumere',
    'Set:sibur', 'Set:mendiane', 'Set:phiras', 'Set:thystame',
    'Incantation',
]
```

Total : ~30 actions discrètes.

## Reward shaping (multi-objectif)

```python
reward = (
    + 0.01      # survival bonus per tick alive
    + 0.5  * delta_food                   # food gained
    + 0.3  * delta_useful_stone           # stones useful for next elevation
    + 5.0  * elevation_success            # +5 if level up
    - 5.0  * death                        # -5 if dead
    + 2.0  * successful_team_incantation  # if incantation with teammates
    + 0.5  * useful_broadcast_received    # if broadcast leads to gather
    + 0.1  * teammate_alive_count_delta   # bonus si teammates restent en vie
)
```

Reward potential-based shaping pour préserver convergence.

## Curriculum learning

Yaml config :
```yaml
stages:
  - name: tiny
    map: [10, 10]
    n_teams: 2
    max_players_per_team: 4
    f: 10000
    n_episodes: 5_000
  - name: small
    map: [20, 20]
    n_teams: 2
    max_players_per_team: 6
    f: 10000
    n_episodes: 10_000
  - name: medium
    map: [50, 50]
    n_teams: 4
    max_players_per_team: 6
    f: 10000
    n_episodes: 20_000
  - name: ref_size
    map: [50, 50]
    n_teams: 4
    max_players_per_team: 6
    f: 100
    n_episodes: 10_000
    eval_every: 500
    eval_vs: zappy_ref
```

## Self-play tournament + ELO

- Pool de checkpoints (top 5 par ELO)
- À chaque eval : tirer 2 policies au hasard pondéré ELO, jouer N matches, mettre à jour ELO
- Système ELO classique (K=24)
- Dashboard streamlit : leaderboard temps réel

## Export & inference C++

1. Entraîné en Python → `torch.jit.script(policy)` → `model.pt`
2. Vérifié via `torch.jit.load` Python + roundtrip check
3. Loaded en C++ :
   ```cpp
   auto module = torch::jit::load("model.pt");
   module.eval();
   auto output = module.forward({obs_tensor}).toTensor();
   ```
4. `model.pt` versionné via git-lfs dans `models/` au moment du release tag

## Broadcast codé team

- Format : `Broadcast: <magic_byte><b64_payload>\n`
- Magic byte : XOR(team_id, fixed_team_secret) — petit secret partagé via fichier `team_codes.json` packagé avec l'IA
- Payload : 1 byte type + N bytes data
- Types codés :
  - `0x01 HELP <x,y>` : appel à l'aide à la position estimée
  - `0x02 HERE <inv_summary>` : "je suis là avec ces ressources"
  - `0x03 READY_LVL <lvl>` : "prêt pour incantation à ce level"
  - `0x04 GATHER <x,y,lvl>` : "rendez-vous à cette tile pour incanter"
- Decoder : si magic ne match pas team_id local → ignore
- Garantit que les broadcasts inter-team ne soient pas exploitables par adversaires

## Tests

- `test_observation.py` : encode/decode roundtrip, shape correcte
- `test_reward.py` : exhaustif sur tous les cas (death, level up, etc.)
- `test_broadcast_codec.cpp` : encode/decode + filtre team
- `test_policy_rule_based.cpp` : sur scenarios YAML simples vérifie que la FSM survit et évolue
- `test_export_roundtrip.py` : Python policy → TorchScript → reload Python → outputs égaux à 1e-5

## Machine de training

- Linux Ubuntu 22.04, 64 GB RAM, RTX (CUDA 12.x)
- Accès SSH pour les 6
- Tmux session permanent `zappy-train`
- Lancements via Makefile :
  ```
  make train CURRICULUM=stages/small EPOCHS=10000 GPUS=1
  make eval CHECKPOINT=models/ckpt_20260612.pt OPPONENT=ref
  make export CHECKPOINT=models/ckpt_20260612.pt OUT=models/model.pt
  ```
- Logs : Weights & Biases (W&B) + Tensorboard local
- Checkpoints : `models/runs/<timestamp>/ckpt_<epoch>.pt`, retention 5 derniers + top 3 ELO
