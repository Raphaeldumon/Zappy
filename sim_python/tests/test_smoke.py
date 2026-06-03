"""Smoke test for the zappy_sim pybind11 module.

Skipped unless the module was built (-DZAPPY_BUILD_SIM=ON) and is importable, so it
never breaks the default Python CI run.
"""

import importlib.util

import pytest

if importlib.util.find_spec("zappy_sim") is None:
    pytest.skip(
        "zappy_sim not built (configure with -DZAPPY_BUILD_SIM=ON)",
        allow_module_level=True,
    )

import zappy_sim  # noqa: E402


def test_reset_and_step():
    sim = zappy_sim.Sim(width=10, height=8, n_agents=4)
    assert sim.width == 10
    assert sim.height == 8
    obs = sim.reset(seed=42)
    assert len(obs) == 4
    obs2 = sim.step([0, 1, 2, 3])
    assert len(obs2) == 4
