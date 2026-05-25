# 07 — Structure du monorepo

## Vue d'ensemble

```
G-YEP-400-RUN-4-1-zappy-3/
├── .editorconfig
├── .clang-format
├── .clang-tidy
├── .gitattributes
├── .gitignore
├── .gitmessage                  ← template commit (Conventional Commits)
├── .pre-commit-config.yaml
├── .github/
│   ├── CODEOWNERS
│   ├── PULL_REQUEST_TEMPLATE.md
│   ├── ISSUE_TEMPLATE/
│   │   ├── bug.md
│   │   ├── feature.md
│   │   └── adr_request.md
│   ├── workflows/
│   │   ├── ci.yml               ← build + lint + tests (matrix Ubuntu/Fedora)
│   │   ├── nightly.yml          ← benchmarks + training short run
│   │   ├── release.yml          ← package release tar.gz
│   │   └── deploy_docs.yml      ← mkdocs build + GitHub Pages
│   └── dependabot.yml
├── Makefile                     ← wrapper conforme normes Epitech
├── CMakeLists.txt               ← root CMake
├── cmake/
│   ├── compile_options.cmake
│   ├── sanitizers.cmake
│   ├── coverage.cmake
│   └── find/*.cmake             ← FindAsio, FindGlslang...
├── conanfile.py    ou vcpkg.json (choix S1 ADR-003)
├── docker/
│   ├── Dockerfile
│   ├── Dockerfile.training
│   ├── docker-compose.yml
│   └── .devcontainer/
│       └── devcontainer.json
├── docs/                        ← MkDocs sources
│   ├── mkdocs.yml
│   ├── index.md
│   ├── ...                       (cf PLAN.md)
│   └── adrs/
│       ├── 000-template.md
│       ├── 001-server-language-cpp17.md
│       └── ...
├── server/                      ← zappy_server (C++17/20)
│   ├── CMakeLists.txt
│   ├── core/                    ← logique pure (utilisée par sim aussi)
│   ├── runtime/                 ← serveur réel : poll, asio, spdlog
│   ├── tests/
│   └── README.md
├── gui/                         ← zappy_gui (C++ + Vulkan)
│   ├── CMakeLists.txt
│   ├── include/
│   ├── src/
│   ├── shaders/
│   ├── assets/
│   ├── tests/
│   └── README.md
├── ai_cpp/                      ← zappy_ai (C++ + libtorch)
│   ├── CMakeLists.txt
│   ├── include/
│   ├── src/
│   ├── tests/
│   └── README.md
├── ai_python/                   ← pipeline training
│   ├── pyproject.toml
│   ├── zappy_train/
│   ├── tests/
│   ├── notebooks/
│   └── README.md
├── sim_python/                  ← pybind11 lib `zappy_sim`
│   ├── CMakeLists.txt
│   ├── src/
│   └── tests/
├── tools/                       ← scripts utilitaires
│   ├── build_assets.sh
│   ├── zrec_inspect.cpp
│   ├── run_integration.sh
│   ├── bench_runner.py
│   ├── elo_dashboard.py
│   └── format_all.sh
├── tests/                       ← integration tests cross-component
│   ├── integration_yaml/
│   │   ├── scenario_basic_game.yaml
│   │   ├── scenario_elevation_lvl2.yaml
│   │   └── ...
│   ├── conformance_sim_vs_runtime/
│   │   └── ...
│   └── perf_benchmarks/
│       └── ...
├── models/                      ← git-lfs : checkpoints PT
│   ├── runs/
│   └── current/
│       └── model.pt
├── grafana/                     ← dashboards JSON
│   └── zappy_overview.json
├── prometheus/
│   └── prometheus.yml           ← config scrape
├── reference/                   ← serveur de référence Epitech (tar non commit)
│   └── README.md                ← instructions extraction `zappy_ref-v3.0.1.tgz`
└── PLAN.md                      ← entry point
```

## Conventions de nommage

### Branches Git
- `main` : prod stable
- `develop` : intégration
- `feature/<scope>-<short-desc>` : ex `feature/server-event-scheduler`
- `release/<version>` : ex `release/0.1.0`
- `hotfix/<scope>-<short-desc>` : ex `hotfix/gui-crash-empty-team`
- `chore/<scope>-<short-desc>` : refactor, doc, deps

### Commits — Conventional Commits

Format obligatoire (commitlint en CI) :
```
<type>(<scope>): <subject>

[optional body]

[optional footer]
```

Types autorisés : `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `revert`.

Scopes autorisés : `server`, `gui`, `ai`, `sim`, `train`, `protocol`, `docs`, `ci`, `build`, `deps`, `infra`.

Exemples :
- `feat(server): add admin socket with token auth`
- `fix(gui): correct torus wrap mapping for vision cone`
- `perf(sim): reduce Sim::step allocations by reusing buffers`
- `docs(adrs): add ADR-005 zrec format`
- `ci(workflows): add cppcheck step to ci.yml`

### Fichiers C++
- Header `.hpp`, source `.cpp`
- snake_case pour fichiers
- PascalCase pour classes, snake_case pour fonctions/variables, SCREAMING_SNAKE pour constexpr et macros
- `#pragma once`
- Namespace racine `zappy::`

### Fichiers Python
- snake_case pour fichiers et fonctions
- PascalCase pour classes
- `mypy --strict` doit passer
- pas de top-level code, tout sous `if __name__ == "__main__"`

### Shaders GLSL
- Extensions `.vert`, `.frag`, `.comp`, `.rgen`, `.rmiss`, `.rchit`, `.glsl` (includes)
- snake_case nommage
- Naming convention layout :
  ```glsl
  // INPUT  : uniforms ubo (set 0 binding 0..)
  // OUTPUT : varyings out_*
  // PUSH   : push constants in struct PushConstants
  ```

## Conventions structure fichiers

Chaque module C++ a :
- `include/zappy/<module>/<file>.hpp` : API publique
- `src/<file>.cpp` : implémentation
- `tests/test_<file>.cpp` : unit tests Catch2

CMake target naming :
- `zappy_<module>` pour les libs (ex `zappy_core`, `zappy_gui_renderer`)
- `zappy_<binary>` pour exec (ex `zappy_server`, `zappy_gui`, `zappy_ai`)
- Tests : `zappy_<module>_tests`

## Git LFS

Tracké via `.gitattributes` :
```
models/**/*.pt   filter=lfs diff=lfs merge=lfs -text
gui/assets/**/*.ktx2  filter=lfs diff=lfs merge=lfs -text
gui/assets/**/*.gltf  filter=lfs diff=lfs merge=lfs -text
gui/assets/**/*.ogg   filter=lfs diff=lfs merge=lfs -text
gui/assets/**/*.wav   filter=lfs diff=lfs merge=lfs -text
```

## Submodules / dépendances

Stratégie : **vcpkg manifest mode** (vcpkg.json) pour C++, **pip + pyproject.toml** pour Python.
Pas de submodule git. Tout est résolu par les package managers.

Détail des deps : voir [`docs/09_appendix/02_dependencies.md`](../09_appendix/02_dependencies.md).
