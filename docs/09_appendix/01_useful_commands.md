# 01 — Commandes utiles (cheatsheet)

## Setup initial

```bash
# Cloner le repo
git clone git@github.com:<org>/G-YEP-400-RUN-4-1-zappy-3.git
cd G-YEP-400-RUN-4-1-zappy-3

# Initialiser submodules (si applicable) + git-lfs
git lfs install
git lfs pull

# Option A : Devcontainer (recommandé)
# Ouvrir dans VS Code → Reopen in Container

# Option B : Install local
./tools/install_deps_ubuntu.sh   # ou install_deps_fedora.sh

# Pre-commit (à faire 1x par dev)
pip install --user pre-commit
pre-commit install
pre-commit install --hook-type commit-msg
pre-commit run --all-files   # premier run, peut prendre ~5 min
```

## Build

```bash
# Build complet via Makefile root (normes Epitech)
make                         # build release des 3 binaires + tests
make re                      # rebuild from scratch
make clean                   # clean objets
make fclean                  # clean tout (binaires inclus)
make tests_run               # build + run unit tests

# Build via CMake directement (plus de contrôle)
cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DZAPPY_BUILD_TESTS=ON \
      -DZAPPY_BUILD_SIM=ON \
      -DZAPPY_HAS_RT=ON
cmake --build build -j$(nproc)

# Variantes de build
cmake -S . -B build_debug -DCMAKE_BUILD_TYPE=Debug -DZAPPY_ENABLE_ASAN=ON
cmake -S . -B build_perf -DCMAKE_BUILD_TYPE=Release -DZAPPY_LTO=ON

# Build uniquement le sim Python
cmake --build build --target zappy_sim_pybind
cd ai_python && pip install -e .
```

## Lancer une partie locale

```bash
# Serveur (terminal 1)
./build/zappy_server \
    -p 4242 \
    -x 30 -y 30 \
    -n red blue green \
    -c 6 \
    -f 100

# GUI (terminal 2)
./build/zappy_gui -p 4242 -h localhost

# AIs rule-based (terminal 3)
for i in {1..6}; do
    ./build/zappy_ai -p 4242 -n red -h localhost --no-model &
done
for i in {1..6}; do
    ./build/zappy_ai -p 4242 -n blue -h localhost --no-model &
done

# Ou via script
./tools/launch_demo_ais.sh
```

## Lancer avec record + admin + métriques

```bash
./build/zappy_server \
    -p 4242 -x 50 -y 50 -n red blue green yellow \
    -c 6 -f 500 \
    --record game_$(date +%s).zrec \
    --admin-token "demo-token" \
    --metrics-port 9090
```

## Mode admin

```bash
# Connecter au socket admin
nc localhost 5242

# Commandes (après auth)
auth demo-token
pause
resume
set f 200
kill 42
spawn thystame 15 15 5
snapshot /tmp/snap.json
reload-config config/server.json
quit
```

## Replay

```bash
# Inspecter un .zrec
./build/tools/zrec_inspect game.zrec --header
./build/tools/zrec_inspect game.zrec --stats
./build/tools/zrec_inspect game.zrec --dump | head -50

# Rejouer dans le GUI
./build/zappy_gui --replay game.zrec

# Replay avec speed control + start offset
./build/zappy_gui --replay game.zrec --speed 4 --start 60
```

## Tests

```bash
# Tous les unit tests C++
ctest --test-dir build --output-on-failure -j$(nproc)

# Filtrer
ctest --test-dir build -R 'world_state' -V

# Tests intégration YAML
./tools/run_integration.sh
./tools/run_integration.sh tests/integration_yaml/scenario_basic_game.yaml

# Tests Python
cd ai_python
pytest -v
pytest --cov=zappy_train --cov-report=html
mutmut run --paths-to-mutate zappy_train/

# Conformance sim vs runtime
./build/tests/conformance_sim_vs_runtime/conformance_sim_vs_runtime
```

## Benchmarks

```bash
# Tous les benchmarks
./tools/bench_runner.py --report bench_$(date +%Y%m%d).json

# Bench specifique
./build/server/tests/bench_event_scheduler --benchmark_filter='.*tick.*'
./build/sim_python/tests/bench_sim --steps 1000000

# Bench training
cd ai_python
python -m zappy_train.training.bench_throughput --workers 16
```

## Training RL

```bash
cd ai_python

# Stage smoke (5 min, debug)
python -m zappy_train.training.train_ppo --config config/ppo_smoke.yaml --max-time-min 5

# Stage 0 (tiny map)
python -m zappy_train.training.train_ppo --config config/curriculum/stage_0_tiny.yaml

# Stage suivant en reprenant checkpoint
python -m zappy_train.training.train_ppo \
    --config config/curriculum/stage_1_small.yaml \
    --resume models/runs/<run_id>/ckpt_latest.pt

# Eval contre ref server
python -m zappy_train.training.eval \
    --opponent ref \
    --model models/current/model.pt \
    --matches 100 \
    --report models/current/eval_report.md

# Self-play tournament
python -m zappy_train.training.self_play \
    --pool models/runs/<run_id>/top_5/ \
    --matches-per-pair 20

# Export TorchScript pour libtorch C++
python -m zappy_train.export.export_torchscript \
    --checkpoint models/runs/<run_id>/ckpt_best.pt \
    --output models/current/model.pt
```

## Docker

```bash
# Build image dev
docker build -t zappy-dev -f docker/Dockerfile .

# Build image training (avec CUDA)
docker build -t zappy-training -f docker/Dockerfile.training .

# Lancer devcontainer
docker compose -f docker/docker-compose.yml up devcontainer

# Lancer stack observabilité (Prometheus + Grafana)
docker compose -f docker/observability.yml up -d

# Open Grafana
xdg-open http://localhost:3000
# (default: admin/admin)

# Open Prometheus
xdg-open http://localhost:9091
```

## Git workflow

```bash
# Nouvelle feature
git checkout develop && git pull --rebase
git checkout -b feature/server-add-elevation

# Commit (commitlint vérifie format)
git commit -m "feat(server): add elevation rules table"

# Push + ouvrir PR
git push -u origin feature/server-add-elevation
gh pr create --base develop --fill

# Sync sa branche avec develop (rebase)
git fetch origin
git rebase origin/develop

# Squash interactif local
git rebase -i origin/develop

# Release tag
git checkout release/0.2.0 && git pull --rebase
git tag -a v0.2.0 -m "Release v0.2.0 — Core MVP complete"
git push origin v0.2.0
```

## Lint / format

```bash
# Tout formater + lint
./tools/format_all.sh

# Check uniquement (CI)
./tools/format_all.sh --check

# Hook pre-commit manuel
pre-commit run --all-files

# Python lint
cd ai_python
ruff check .
ruff format .
black --check .
mypy --strict zappy_train

# C++ clang-tidy seul
clang-tidy -p build server/core/world_state.cpp
```

## Docs

```bash
# Servir docs MkDocs en local
mkdocs serve
xdg-open http://localhost:8000

# Build statique
mkdocs build --strict

# Build doxygen
doxygen Doxyfile
xdg-open doxygen_output/html/index.html

# Build mermaid diagrams export svg (rarement nécessaire)
mmdc -i diagram.mmd -o diagram.svg
```

## Profilage

```bash
# CPU profile serveur
perf record -g ./build/zappy_server -p 4242 -x 50 -y 50 -n red -c 6 -f 500
perf report

# Trace flame graph
perf script | ./tools/flamegraph.pl > flamegraph.svg

# GPU profile GUI
# RenderDoc :
./build/zappy_gui &
# puis Attach via RenderDoc UI
# F12 pour capture frame

# Tracy real-time
cmake -S . -B build -DZAPPY_USE_TRACY=ON
# lancer client Tracy en parallèle
./build/zappy_gui

# Memory
valgrind --tool=massif ./build/zappy_server -p 4242 -x 10 -y 10 -n red -c 4 -f 100
ms_print massif.out.<pid>
```

## Misc

```bash
# Voir tous les ADRs
ls docs/adrs/

# Voir last 10 commits formatés
git log --oneline -10

# Diff des changements par rapport à develop
git diff develop...HEAD

# Vérifier coverage gcovr
gcovr --root . --html --html-details -o coverage.html
xdg-open coverage.html

# Killer tous les processes Zappy d'un coup
pkill -f zappy_server || true
pkill -f zappy_gui || true
pkill -f zappy_ai || true

# Suivre logs serveur (spdlog formaté)
./build/zappy_server -p 4242 ... 2>&1 | tee server.log | grep -E '(ERROR|WARN)'

# Quick test handshake
echo "GRAPHIC" | nc -q 1 localhost 4242

# Generate sample replay pour tests
./tools/generate_sample_replay.sh tests/fixtures/sample.zrec
```
