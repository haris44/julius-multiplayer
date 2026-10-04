#!/usr/bin/env bash
# Checks that the real game (real Caesar III data) and simtool (test stubs) compute exactly the
# same simulation: same state checksum every STEP ticks. Needs the game data (C3_DATA_DIR).
# Known expected difference: when a classic victory is reached, the test stub automatically
# "continues to govern" while the real game waits on the victory dialog.
#
# Usage: tools/cross-check.sh SAVE [TICKS] [STEP]     (SAVE relative to test/data)
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SAVE=${1:?usage: tools/cross-check.sh SAVE [TICKS] [STEP]}
TICKS=${2:-2000}
STEP=${3:-100}
OUT="$ROOT/build/automation"
mkdir -p "$OUT"

script="$OUT/cross-check.txt"
{
    echo "load test/data/$SAVE"
    echo "rules mp"
    echo "pause"
    echo "checksum"
    for ((t = STEP; t <= TICKS; t += STEP)); do
        echo "ticks $STEP"
        echo "checksum"
    done
    echo "quit"
} > "$script"

(cd "$ROOT" && tools/run-automation.sh "$script" 300) 2>&1 | grep "checksum:" | sed -E 's/^.*checksum: //' > "$OUT/real.trace"
(cd "$ROOT/build/test" && ./simtool --mp trace "$SAVE" "$TICKS" "$STEP") | awk '{print $2}' > "$OUT/sim.trace"

first_diff=$(paste "$OUT/real.trace" "$OUT/sim.trace" | awk -v step="$STEP" '$1 != $2 {print (NR - 1) * step; exit}')
if [ -n "$first_diff" ]; then
    echo "DIFFERENT from tick $first_diff (between $((first_diff - STEP)) and $first_diff)"
    exit 1
fi
echo "IDENTICAL over $TICKS ticks ($(wc -l < "$OUT/real.trace" | tr -d ' ') checksums)"
