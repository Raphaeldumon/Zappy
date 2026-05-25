# Zappy — G-YEP-400

Network strategy game: a **server**, a **Vulkan GUI**, and an **AI client**, built by a
team of 6 over 4 weeks. Full plan and docs live in [`PLAN.md`](PLAN.md) and [`docs/`](docs/).

## Components

| Path         | Binary / package | Owners        | Stack                         |
|--------------|------------------|---------------|-------------------------------|
| `server/`    | `zappy_server`   | P1 Léa, P2 Marc   | C++20 (core lib + runtime)    |
| `gui/`       | `zappy_gui`      | P3 Sami, P4 Inès  | C++20 + Vulkan 1.3            |
| `ai_cpp/`    | `zappy_ai`       | P5 Théo, P6 Yanis | C++20 (libtorch inference)    |
| `ai_python/` | `zappy_train`    | P5 Théo           | Python + PyTorch + RLlib      |
| `sim_python/`| `zappy_sim`      | P6 Yanis          | pybind11 over `libzappy_core` |
| `protocol/`  | `zappy_protocol` | shared           | C++20 header-only contract    |

## Quick start

```bash
# Build everything that has its dependencies available (server + ai always; gui stub).
make

# Run the binaries
./zappy_server --help
./zappy_ai --help
./zappy_gui --help

# Tests
make tests_run
```

### Build the real Vulkan GUI

The default build ships a **GUI stub** so the repo always compiles. To build the real
Vulkan renderer you need a Vulkan 1.3 SDK + `glfw3` (via vcpkg, see `vcpkg.json`):

```bash
make gui_on            # = cmake -DZAPPY_BUILD_GUI=ON ...
```

### CMake options

| Option | Default | Meaning |
|--------|:-------:|---------|
| `ZAPPY_BUILD_SERVER` | ON | build `zappy_server` |
| `ZAPPY_BUILD_AI` | ON | build `zappy_ai` |
| `ZAPPY_BUILD_GUI` | ON | build `zappy_gui` (real Vulkan if deps found, else stub) |
| `ZAPPY_BUILD_TESTS` | ON | build + register CTest unit tests |
| `ZAPPY_BUILD_SIM` | OFF | build the pybind11 `zappy_sim` module |
| `ZAPPY_ENABLE_SANITIZERS` | OFF | ASan + UBSan |
| `ZAPPY_ENABLE_COVERAGE` | OFF | gcov flags |

## Where do I start? (per role)

Read your file in [`docs/07_calendar_per_person/`](docs/07_calendar_per_person/), then your
module's `README.md`. The Sprint 1 plan is [`docs/06_calendar/sprints/sprint_1_w1_foundations.md`](docs/06_calendar/sprints/sprint_1_w1_foundations.md).

## Conventions

- Conventional Commits (`feat(server): ...`), GitFlow branches — see `docs/03_process/`.
- C++ formatted with `.clang-format`, Python with `ruff`. Run `make format`.
- Docs in French, code/commits/PRs in English.
