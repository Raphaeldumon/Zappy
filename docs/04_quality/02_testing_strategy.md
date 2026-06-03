# 02 — Stratégie de tests

## Pyramide

```
                  ┌──────────────────────────┐
                  │   Manual / soutenance    │   ← démo live, vidéo
                  └──────────────────────────┘
                  ┌──────────────────────────┐
                  │  E2E / Smoke (GUI)       │   ← screenshot diff, démarrage 5s
                  └──────────────────────────┘
                ┌──────────────────────────────┐
                │  Integration AI↔server YAML   │  ← scenarios, 30+ tests
                └──────────────────────────────┘
              ┌──────────────────────────────────┐
              │  Conformance sim vs runtime       │ ← seeded tests bit-exact
              └──────────────────────────────────┘
            ┌──────────────────────────────────────┐
            │  Benchmarks (perf regression nightly) │
            └──────────────────────────────────────┘
          ┌──────────────────────────────────────────┐
          │  Unit tests (Catch2 C++, pytest Python)   │ ← masse, vite, en CI sur PR
          └──────────────────────────────────────────┘
```

## Unit tests

### C++ — Catch2 v3

Convention : pour `src/foo.cpp`, le test est `tests/test_foo.cpp`.

Coverage target : **70%** sur `server/core/`, `gui/include/zappy/gui/scene/`, `gui/include/zappy/gui/net/`, `ai_cpp/`, `sim_python/src/`.

Mesure : `gcovr` (gcc) ou `llvm-cov` (clang), uploadé Codecov.

Exemple :
```cpp
// tests/test_world_state.cpp
#include <catch2/catch_test_macros.hpp>
#include "zappy/server/world_state.hpp"

TEST_CASE("WorldState wraps toroidal", "[world_state]") {
    zappy::WorldState w(10, 10);
    auto& tile_origin = w.at(0, 0);
    auto& tile_wrap   = w.at(10, 10);
    CHECK(&tile_origin == &tile_wrap);
}

TEST_CASE("Vision cone level 1", "[world_state][vision]") {
    zappy::WorldState w(20, 20);
    auto player = w.add_player("red", 5, 5, zappy::Orientation::N);
    auto cone = w.look(player);
    REQUIRE(cone.size() == 4);  // 1 + 3 tiles for level 1
}
```

### Python — pytest + mutmut

Pour `ai_python/zappy_train/foo.py`, test = `ai_python/tests/test_foo.py`.

Coverage target : **80%** sur `zappy_train/`.

`mutmut` : tests de mutation pour s'assurer que les tests détectent vraiment les bugs.

```python
# tests/test_reward.py
import pytest
from zappy_train.env.reward import compute_reward

def test_reward_survival_bonus_per_tick():
    r = compute_reward(prev_state=stub_state(food=5), new_state=stub_state(food=5),
                       action_id=0, alive=True, just_died=False)
    assert r == pytest.approx(0.01)

def test_reward_death_penalty():
    r = compute_reward(prev_state=stub_state(food=0), new_state=stub_state(food=0),
                       action_id=0, alive=False, just_died=True)
    assert r == pytest.approx(-5.0)
```

## Tests d'intégration AI↔server

Scenarios YAML dans `tests/integration_yaml/` :
```yaml
# scenario_basic_game.yaml
name: basic_game_two_teams_finish
seed: 42
server:
  width: 10
  height: 10
  teams: [red, blue]
  clients_per_team: 4
  f: 1000
ai:
  - { team: red, count: 4, policy: rule-based }
  - { team: blue, count: 4, policy: rule-based }
max_duration_sec: 60
expectations:
  - assertion: any_team_wins
  - assertion: no_crashes
  - assertion: all_actions_acknowledged_in_time
```

Runner : `tools/run_integration.sh` lance le serveur, les AIs, attend la fin, vérifie les expectations.

Output : code 0 si tout PASS, sinon code 1 + logs.

CI : tourne sur chaque PR. Timeout 5 min total.

## Conformance sim vs runtime

**Test critique** : pour un seed donné et une suite d'actions scriptées, l'état du `WorldState` après N steps doit être **bit-à-bit identique** entre :
- `libzappy_sim` (Python via pybind11)
- `zappy_server` runtime (lancé avec `-DZAPPY_DETERMINISTIC=ON --no-sleep`)

Implémentation :
```cpp
// tests/conformance_sim_vs_runtime/main.cpp
int main() {
    for (auto& scenario : load_scenarios("scenarios/conformance/*.yaml")) {
        auto sim_state = run_sim(scenario);
        auto rt_state  = run_runtime(scenario);
        REQUIRE(sim_state.snapshot_json() == rt_state.snapshot_json());
    }
}
```

Si KO → CI rouge, on n'avance pas.

## Smoke test GUI

`tools/gui_smoke.sh` :
1. Lance `xvfb-run -a ./zappy_gui --replay tests/fixtures/sample.zrec --headless --duration 5 --screenshot out.png`
2. Compare `out.png` à `tests/fixtures/sample_screenshot.png` via `pixelmatch` (tolérance 5% pixels diff, threshold 0.1)
3. PASS si dans la tolérance

Limité pour le rendu GPU sur CI car validation Vulkan + xvfb peut être instable. En cas d'instabilité : marquer le test `flaky-allow` et lancer en nightly.

## Benchmarks (perf regression)

Catégories :
- **FPS GUI** : map 200x200, 24 players, 4 teams, charge cinematic 30s
- **Server ticks/sec** : map 200x200, 24 AIs rule-based, f=500
- **Sim steps/sec** : `libzappy_sim` map 20x20 et 50x50, single env et batch 64 envs
- **Training samples/sec** : RLlib PPO smoke run 5 min, mesure throughput
- **Inference latency C++ libtorch** : forward pass single obs

Outils :
- C++ : Google Benchmark, intégré au CMake
- Python : `pytest-benchmark`

Nightly tournent et compare avec moyenne mobile 7 jours. Drop >10% → CI rouge.

## Coverage targets

| Composant | Outil | Cible |
|-----------|-------|-------|
| server/core/ | gcovr | 70% |
| gui/ | gcovr | 50% (rendu hard à tester) |
| ai_cpp/ | gcovr | 70% |
| sim_python/ | gcovr | 70% |
| ai_python/zappy_train/ | pytest-cov | 80% |

## Tests "flaky" — politique

- 3 fail consécutifs sur main → on désactive le test temporairement, ouvre une issue `bug:flaky-test`
- L'issue est traitée dans le sprint suivant
- Pas de "retry until pass" en CI (cache les vrais bugs)

## Test data / fixtures

- `tests/fixtures/` : assets de test (sample.zrec, screenshots, YAML scenarios)
- Tracké via git-lfs si > 100KB
- Pas de données sensibles, pas de secrets

## Mocks vs real

- **Server core** : pas de mock, tout est testable en pur C++ deterministe
- **Server runtime (network)** : mocks pour les sockets via interface `INetworkLayer`
- **GUI** : mock `gui::INetClient` pour tester les passes de rendu sans serveur
- **AI** : peut être testé contre `libzappy_sim` directement (pas besoin de runtime serveur)

## Tests cross-platform

- Ubuntu 22.04 + Fedora 39 dans la matrix CI
- macOS / Windows : **hors scope** (sujet Linux only)

## Quand exécuter quoi (résumé)

| Quand | Tests exécutés |
|-------|----------------|
| pre-commit local | lint + format + cppcheck + glslangValidator |
| `make test` local | unit C++ + pytest (rapide, ~30s) |
| Push PR (CI) | unit + integration YAML + conformance + lint + commitlint + docs build |
| Nightly (cron 02:00) | benchmarks + training short + integration long suite + conformance long |
| Release tag | tous + manual smoke démo |
