"""Zappy reinforcement-learning training package.

S1 scope: importable package with a stub multi-agent environment and reward shaping,
so the RLlib wiring (P5) and the C++/Python encoding contract can be developed in
parallel. No heavy deps are imported at module load time.
"""

from zappy_train.encoding import NUM_ACTIONS, OBS_DIM, Action

__all__ = ["NUM_ACTIONS", "OBS_DIM", "Action"]
__version__ = "0.1.0"
