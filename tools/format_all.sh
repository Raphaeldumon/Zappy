#!/usr/bin/env bash
# Format all C++ and Python sources. Run from anywhere in the repo.
#
#   tools/format_all.sh           # format in place (default)
#   tools/format_all.sh --check   # verify only, non-zero exit if reformat needed (CI)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CHECK=0
if [[ "${1:-}" == "--check" ]]; then
    CHECK=1
fi

status=0

cpp_files() {
    git ls-files '*.cpp' '*.hpp' '*.c' '*.h' 2>/dev/null | grep -Ev '^(build|reference)/' || true
}

if [[ $CHECK -eq 1 ]]; then
    echo "==> clang-format --check (C++)"
    if command -v clang-format >/dev/null 2>&1; then
        if ! cpp_files | xargs -r clang-format --dry-run -Werror; then
            echo "   C++ formatting issues found (run: tools/format_all.sh)" >&2
            status=1
        fi
    else
        echo "   clang-format not found, skipping"
    fi

    echo "==> ruff --check (Python)"
    if command -v ruff >/dev/null 2>&1; then
        ruff format --check ai_python sim_python || status=1
        ruff check ai_python sim_python || status=1
    else
        echo "   ruff not found, skipping (pip install ruff)"
    fi

    exit $status
fi

echo "==> clang-format (C++)"
if command -v clang-format >/dev/null 2>&1; then
    # Only our source trees; skip build/ and third-party.
    cpp_files | xargs -r clang-format -i
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
