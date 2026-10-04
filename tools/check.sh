#!/usr/bin/env bash
# Build everything and run all automated checks.
# Must pass before every commit (see CLAUDE.md).
#
# Usage: tools/check.sh
# Env:   BUILD_DIR (default: build)
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR=${BUILD_DIR:-build}
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

# The development machine has very little free disk space: refuse to run when nearly full.
FREE_MB=$(df -Pm . | awk 'NR==2 {print $4}')
if [ "${FREE_MB:-0}" -lt 300 ]; then
    echo "ERROR: only ${FREE_MB} MB free on disk, free some space first." >&2
    exit 2
fi

tools/check-determinism.sh

if [ ! -f "$BUILD_DIR/CMakeCache.txt" ]; then
    cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo
fi
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "== Parity tests (autopilot vs original Caesar III saves) =="
(cd "$BUILD_DIR" && ctest -j"$JOBS" --output-on-failure)

echo "== All checks passed =="
