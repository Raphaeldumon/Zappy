# 02 — CI/CD GitHub Actions

## Vue d'ensemble

3 workflows + dependabot :

| Workflow | Trigger | Durée | But |
|----------|---------|-------|-----|
| `ci.yml` | push, PR | ~10 min | build + lint + tests + commitlint |
| `nightly.yml` | cron 02:00 UTC | ~2 h | benchmarks + training short + conformance long |
| `release.yml` | tag `v*` | ~15 min | package tar.gz + GitHub Release |
| `deploy_docs.yml` | push develop / main (docs/**) | ~3 min | MkDocs build + GitHub Pages deploy |
| `dependabot.yml` | weekly | — | PR updates deps |

## `.github/workflows/ci.yml`

```yaml
name: CI

on:
  push:
    branches: [develop, main]
  pull_request:
    branches: [develop, main]

concurrency:
  group: ci-${{ github.ref }}
  cancel-in-progress: true

jobs:
  commitlint:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
        with: { fetch-depth: 0 }
      - uses: wagoid/commitlint-github-action@v6

  build-and-test:
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-22.04, fedora-39]
        compiler: [gcc-13, clang-17]
    runs-on: ${{ matrix.os }}
    container:
      image: ghcr.io/${{ github.repository_owner }}/zappy-ci:${{ matrix.os }}-${{ matrix.compiler }}
      options: --user 1001
    steps:
      - uses: actions/checkout@v4
        with: { lfs: true, submodules: false }

      - name: Cache vcpkg
        uses: actions/cache@v4
        with:
          path: ~/.cache/vcpkg
          key: vcpkg-${{ matrix.os }}-${{ matrix.compiler }}-${{ hashFiles('vcpkg.json') }}

      - name: Configure CMake
        run: cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DZAPPY_BUILD_TESTS=ON -DZAPPY_BUILD_SIM=ON

      - name: Build
        run: cmake --build build -j$(nproc)

      - name: Lint (clang-format, clang-tidy, cppcheck)
        run: ./tools/format_all.sh --check
        if: matrix.compiler == 'clang-17'

      - name: Python lint (ruff, mypy, black)
        run: |
          cd ai_python
          ruff check .
          mypy --strict zappy_train
          black --check .
        if: matrix.os == 'ubuntu-22.04' && matrix.compiler == 'gcc-13'

      - name: Unit tests (Catch2)
        run: ctest --test-dir build --output-on-failure -j$(nproc)

      - name: Pytest
        run: |
          cd ai_python
          pytest --cov=zappy_train --cov-report=xml -n auto
        if: matrix.os == 'ubuntu-22.04' && matrix.compiler == 'gcc-13'

      - name: Integration tests (AI ↔ server YAML scenarios)
        run: ./tools/run_integration.sh
        timeout-minutes: 5

      - name: Conformance sim vs runtime
        run: ./build/tests/conformance_sim_vs_runtime/conformance_sim_vs_runtime

      - name: Upload coverage
        if: matrix.os == 'ubuntu-22.04' && matrix.compiler == 'gcc-13'
        uses: codecov/codecov-action@v4
        with: { files: ./coverage.xml,./build/coverage_cpp.info }
```

## `.github/workflows/nightly.yml`

```yaml
name: Nightly

on:
  schedule:
    - cron: '0 2 * * *'  # 02:00 UTC daily
  workflow_dispatch:

jobs:
  benchmarks:
    runs-on: self-hosted-gpu  # runner avec RTX
    steps:
      - uses: actions/checkout@v4
      - name: Build optimized
        run: |
          cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DZAPPY_BENCHMARKS=ON
          cmake --build build -j
      - name: Run benchmarks
        run: ./tools/bench_runner.py --report bench_$(date +%Y%m%d).json
      - name: Upload benchmark report
        uses: actions/upload-artifact@v4
        with: { name: bench-report, path: bench_*.json }
      - name: Regression check (compare last 7 days)
        run: ./tools/bench_compare.py --history 7 --fail-threshold 10
      - name: Post to Discord
        run: ./tools/post_discord.sh bench_*.json
        env: { DISCORD_WEBHOOK: ${{ secrets.DISCORD_WEBHOOK }} }

  training-smoke:
    runs-on: self-hosted-gpu
    steps:
      - uses: actions/checkout@v4
      - name: Build sim
        run: |
          cmake -S . -B build -DZAPPY_BUILD_SIM=ON -DCMAKE_BUILD_TYPE=Release
          cmake --build build -j --target zappy_sim_pybind
      - name: Pip install
        run: |
          cd ai_python
          pip install -e .
      - name: Train 5 min PPO smoke
        run: |
          cd ai_python
          python -m zappy_train.training.train_ppo \
            --config config/ppo_smoke.yaml \
            --max-time-min 5
      - name: Upload smoke checkpoint
        uses: actions/upload-artifact@v4
        with: { name: train-smoke-ckpt, path: ai_python/runs/smoke/* }
```

## `.github/workflows/release.yml`

```yaml
name: Release

on:
  push:
    tags: ['v*']

jobs:
  package:
    runs-on: ubuntu-22.04
    steps:
      - uses: actions/checkout@v4
        with: { lfs: true }
      - name: Build release
        run: |
          cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
          cmake --build build -j --target zappy_server zappy_gui zappy_ai
      - name: Package
        run: |
          mkdir -p dist/zappy-${{ github.ref_name }}
          cp build/zappy_server build/zappy_gui build/zappy_ai dist/zappy-${{ github.ref_name }}/
          cp -r gui/assets dist/zappy-${{ github.ref_name }}/
          cp -r models/current dist/zappy-${{ github.ref_name }}/models
          cp Makefile dist/zappy-${{ github.ref_name }}/
          tar czf zappy-${{ github.ref_name }}.tar.gz -C dist zappy-${{ github.ref_name }}
      - name: Generate changelog
        run: ./tools/changelog.sh > CHANGELOG.md
      - uses: softprops/action-gh-release@v2
        with:
          files: zappy-${{ github.ref_name }}.tar.gz
          body_path: CHANGELOG.md
          draft: false
```

## `.github/workflows/deploy_docs.yml`

```yaml
name: Deploy Docs

on:
  push:
    branches: [develop, main]
    paths: ['docs/**', 'mkdocs.yml']

jobs:
  docs:
    runs-on: ubuntu-22.04
    permissions: { contents: read, pages: write, id-token: write }
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: '3.11' }
      - name: Install MkDocs
        run: pip install mkdocs-material mkdocs-mermaid2-plugin mkdocs-git-revision-date-localized-plugin
      - name: Build doxygen API docs
        run: |
          sudo apt-get install -y doxygen graphviz
          doxygen Doxyfile
          mkdir -p docs/api_reference && cp -r doxygen_output/html/* docs/api_reference/
      - name: Build MkDocs
        run: mkdocs build --strict
      - uses: actions/upload-pages-artifact@v3
        with: { path: site }
      - uses: actions/deploy-pages@v4
```

## Self-hosted runner GPU (training)

Setup sur la machine 64GB+RTX (P6 responsable) :
- Installer GitHub Actions runner avec label `self-hosted-gpu`
- Drivers NVIDIA + CUDA 12.x
- Docker + nvidia-container-toolkit
- Vulkan SDK 1.3.x

## Cache strategy

- **vcpkg** : `actions/cache@v4`, key = OS + compiler + hash(vcpkg.json)
- **pip** : `actions/cache@v4`, key = OS + hash(pyproject.toml)
- **ccache** (C++ build) : `ccache-action`, ~70% gain sur rebuilds
- **MkDocs** : `mkdocs-git-revision-date-localized-plugin` requiert fetch-depth: 0 + cache `~/.cache/pip`

## Notifications

- Discord webhook channel `#ci-alerts` :
  - CI rouge sur `main` ou `develop` → 🚨 ping on-call
  - Benchmark regression → 📉 alerte
  - Release publiée → 🚀 announcement

## Secrets requis

| Secret | Usage |
|--------|-------|
| `DISCORD_WEBHOOK` | Notif Discord |
| `CODECOV_TOKEN` | Upload coverage |
| `WANDB_API_KEY` | Training logs |

## Definition of "CI verte"

- Tous les jobs `ci.yml` GREEN
- Pas de warning C++ (treated as error)
- Pas de warning Python ruff/mypy
- Couverture >= seuils (70% C++, 80% Python)
- Conformance sim vs runtime PASS
- Tests d'intégration AI↔server PASS
