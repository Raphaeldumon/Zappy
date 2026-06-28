#!/usr/bin/env bash
# Connection-limit / stress tests for zappy_server.
#
# Builds zappy_server if needed, then runs tools/test_connection_limits.py:
#   - fills one team past its -c capacity (excess refused, server alive)
#   - opens MAX_TOTAL_CLIENTS+ raw connections (ceiling enforced, no crash)
# Exit 0 if both tests PASS, 1 otherwise.
#
#   tools/run_stress.sh
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

exec python3 tools/test_connection_limits.py --server "$SERVER" "$@"
