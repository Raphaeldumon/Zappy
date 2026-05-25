"""Reward shaping (P5 owns; ADR-017).

Stdlib-only so it's unit-testable without the RL stack. The weights here are a
starting point — tune against the eval pipeline.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class RewardWeights:
    elevation: float = 10.0   # reaching the next level
    survival: float = 0.01    # per tick alive
    death: float = -5.0       # dying
    food_gain: float = 0.5    # picking up food
    win: float = 100.0        # team reaches the win condition (6 players at lvl 8)


def shaped_reward(
    *,
    leveled_up: bool,
    alive: bool,
    died: bool,
    food_delta: int,
    won: bool,
    weights: RewardWeights = RewardWeights(),
) -> float:
    r = 0.0
    if leveled_up:
        r += weights.elevation
    if alive:
        r += weights.survival
    if died:
        r += weights.death
    if food_delta > 0:
        r += weights.food_gain * food_delta
    if won:
        r += weights.win
    return r
