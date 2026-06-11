"""Stub multi-agent Zappy environment.

S1: returns correctly-shaped random observations so the RLlib pipeline (P5) can run
end-to-end before the real simulator (sim_python via libzappy_core) is wired in.

Deliberately dependency-free (stdlib only) so `import` and tests work without torch/
ray/pettingzoo installed. P5 swaps the base class to pettingzoo.ParallelEnv once the
RL deps are installed (`pip install -e "ai_python[rl]"`).
"""

from __future__ import annotations

import random
from dataclasses import dataclass, field

from zappy_train.encoding import NUM_ACTIONS, OBS_DIM


@dataclass
class StepResult:
    observations: dict[str, list[float]]
    rewards: dict[str, float]
    terminations: dict[str, bool]
    truncations: dict[str, bool]
    infos: dict[str, dict] = field(default_factory=dict)


class ZappyParallelEnv:
    """Minimal PettingZoo-style parallel env stub."""

    def __init__(self, n_agents: int = 4, max_steps: int = 1000, seed: int | None = None):
        self.n_agents = n_agents
        self.max_steps = max_steps
        self._rng = random.Random(seed)
        self._step = 0
        self.agents: list[str] = [f"drone_{i}" for i in range(n_agents)]

    @property
    def num_actions(self) -> int:
        return NUM_ACTIONS

    @property
    def obs_dim(self) -> int:
        return OBS_DIM

    def _obs(self) -> dict[str, list[float]]:
        return {a: [self._rng.random() for _ in range(OBS_DIM)] for a in self.agents}

    def reset(self, seed: int | None = None) -> dict[str, list[float]]:
        if seed is not None:
            self._rng.seed(seed)
        self._step = 0
        self.agents = [f"drone_{i}" for i in range(self.n_agents)]
        return self._obs()

    def step(self, actions: dict[str, int]) -> StepResult:
        for a, act in actions.items():
            if not 0 <= int(act) < NUM_ACTIONS:
                raise ValueError(f"action {act} for {a} out of range [0,{NUM_ACTIONS})")
        self._step += 1
        done = self._step >= self.max_steps
        return StepResult(
            observations=self._obs(),
            rewards={a: self._rng.uniform(-1.0, 1.0) for a in self.agents},
            terminations={a: False for a in self.agents},
            truncations={a: done for a in self.agents},
        )
