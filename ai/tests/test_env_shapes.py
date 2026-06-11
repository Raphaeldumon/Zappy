import pytest

from zappy_train import NUM_ACTIONS, OBS_DIM
from zappy_train.env import ZappyParallelEnv


def test_reset_shapes():
    env = ZappyParallelEnv(n_agents=4, seed=0)
    obs = env.reset(seed=0)
    assert set(obs.keys()) == {f"drone_{i}" for i in range(4)}
    for vec in obs.values():
        assert len(vec) == OBS_DIM


def test_step_shapes_and_done():
    env = ZappyParallelEnv(n_agents=2, max_steps=3, seed=1)
    env.reset(seed=1)
    actions = {a: 0 for a in env.agents}
    for _ in range(2):
        r = env.step(actions)
        assert not all(r.truncations.values())
    r = env.step(actions)
    assert all(r.truncations.values())  # truncated at max_steps
    for vec in r.observations.values():
        assert len(vec) == OBS_DIM


def test_invalid_action_raises():
    env = ZappyParallelEnv(n_agents=1, seed=2)
    env.reset()
    with pytest.raises(ValueError):
        env.step({"drone_0": NUM_ACTIONS + 5})
