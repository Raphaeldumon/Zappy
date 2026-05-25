#!/usr/bin/env bash
# Format all C++ and Python sources in place. Run from anywhere in the repo.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "==> clang-format (C++)"
if command -v clang-format >/dev/null 2>&1; then
    # Only our source trees; skip build/ and third-party.
    git ls-files '*.cpp' '*.hpp' '*.c' '*.h' 2>/dev/null \
        | grep -Ev '^(build|reference)/' \
        | xargs -r clang-format -i
else
    echo "   clang-format not found, skipping"
fi

echo "==> ruff (Python)"
if command -v ruff >/dev/null 2>&1; then
    ruff format ai_python sim_python 2>/dev/null || true
    ruff check --fix ai_python sim_python 2>/dev/null || true
else
    echo "   ruff not found, skipping (pip install ruff)"
fi

echo "==> done"
