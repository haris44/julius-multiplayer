#!/usr/bin/env bash
# Runs the real game without any window, driven by an automation script
# (synthetic input, fast-forward, screenshots). See doc/mp/TESTING.md.
#
# Usage: tools/run-automation.sh SCRIPT [TIMEOUT_SECONDS]
#   Relative paths inside SCRIPT are resolved against the current directory.
# Env:   C3_DATA_DIR  original game data (default: ../donnees-c3 next to the repo)
#        BUILD_DIR    build directory (default: build)
set -euo pipefail

SCRIPT=${1:?usage: tools/run-automation.sh SCRIPT [TIMEOUT_SECONDS]}
TIMEOUT=${2:-120}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-$ROOT/build}
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}

BIN="$BUILD_DIR/julius.app/Contents/MacOS/julius"
[ -x "$BIN" ] || BIN="$BUILD_DIR/julius"
if [ ! -x "$BIN" ]; then
    echo "ERROR: julius binary not found in $BUILD_DIR, run tools/check.sh first" >&2
    exit 2
fi
if [ ! -f "$DATA_DIR/c3.eng" ]; then
    echo "ERROR: Caesar III data not found in $DATA_DIR (set C3_DATA_DIR)" >&2
    exit 2
fi
mkdir -p "$BUILD_DIR/automation"

# Never open a real window, never play sound.
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
# perl's alarm survives exec: kills the game if it hangs (macOS has no `timeout`).
exec perl -e 'alarm shift @ARGV; exec @ARGV or die "exec failed: $!"' \
    "$TIMEOUT" "$BIN" --windowed --automation "$SCRIPT" "$DATA_DIR"
