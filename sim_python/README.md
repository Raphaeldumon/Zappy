# sim_python/ — `zappy_sim`

Owner: **P6 Yanis**.

A pybind11 module that wraps **libzappy_core** (the server's pure game logic) so the
RL pipeline can run thousands of headless games per second. Because it reuses the
same C++ logic as the real server, the simulator and server cannot diverge — that's
verified by `tests/conformance_sim_vs_runtime/`.

## Build (opt-in)

Needs `pybind11` (vcpkg `sim` feature). Off by default:

```bash
cmake -S . -B build -DZAPPY_BUILD_SIM=ON
cmake --build build -j
PYTHONPATH=build/bin python -c "import zappy_sim; s=zappy_sim.Sim(); print(s.reset(seed=42))"
pytest sim_python/tests -q          # auto-skips if not built
```

## Where to start (Sprint 1)

- Bind the real `WorldState` step (apply actions, advance the `EventScheduler`).
- Keep the observation layout identical to `ai_python/zappy_train/encoding.py`.
- Add the first conformance scenario (move forward N times, compare server vs sim
  snapshots) under `tests/conformance_sim_vs_runtime/`.
