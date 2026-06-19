#!/usr/bin/env bash
# Integration tests: AI <-> server <-> GUI protocol smoke.
#
# Builds zappy_server if needed, then runs every scenario in
# tests/integration_yaml/ via tools/run_integration.py.
# Exit 0 if all scenarios PASS (SKIPs allowed), 1 otherwise.
#
#   tools/run_integration.sh                       # all scenarios
#   tools/run_integration.sh tests/integration_yaml/scenario_basic_game.yaml
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BUILD_DIR="${BUILD_DIR:-build}"
SERVER="$BUILD_DIR/bin/zappy_server"

if [ ! -x "$SERVER" ]; then
    echo "==> building zappy_server"
    cmake -S . -B "$BUILD_DIR" >/dev/null
    cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)" --target zappy_server
fi

exec python3 tools/run_integration.py --server "$SERVER" "$@"
