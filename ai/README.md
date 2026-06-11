# ai_python/ — `zappy_train`

Owner: **P5 Théo** (RL training).

The RL training pipeline (PettingZoo + RLlib PPO). It trains the policy that
`ai_cpp/zappy_ai` loads at inference time (`models/current/model.pt`).

## Layout

```
zappy_train/
  encoding.py              action/obs contract — MUST match ai_cpp/ (ADR-coordinated)
  reward.py                reward shaping (ADR-017)
  env/pettingzoo_env.py    stub multi-agent env (random, stdlib-only) -> real sim later
  training/train_ppo.py    PPO entry point (--debug runs dependency-free)
tests/                     pytest (stdlib-only, no GPU stack needed)
```

## Setup

```bash
pip install -e ai_python              # core package (no heavy deps)
pip install -e "ai_python[rl]"        # + torch / ray[rllib] / pettingzoo / wandb
pip install -e "ai_python[dev]"       # + pytest / ruff / mypy

pytest ai_python/tests -q
python -m zappy_train.training.train_ppo --debug --max-steps 1000
```

## Where to start (Sprint 1)

- Finalize the obs/action encoding in `encoding.py` **with P6** (it mirrors the C++
  side) — D3 milestone, then freeze it.
- Swap the env stub to a real `pettingzoo.ParallelEnv` backed by `zappy_sim`
  (sim_python) once it returns real state.
- Wire RLlib PPO config in `train_ppo.py`, log to W&B/TensorBoard.

The reward and encoding are protocol-adjacent: changes need an ADR + P5/P6 sign-off.
