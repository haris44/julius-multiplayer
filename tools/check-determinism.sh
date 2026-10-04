#!/usr/bin/env bash
# Fails if simulation code uses floating point, the clock or rand(): every computer must compute
# exactly the same game (doc/mp/DESIGN.md §3.6). Called by tools/check.sh.
set -euo pipefail
cd "$(dirname "$0")/.."

SIMULATION="src/building src/city src/empire src/figure src/figuretype src/map src/scenario src/mp
    src/core/random.c src/core/calc.c src/game/tick.c src/game/time.c src/game/difficulty.c
    src/game/rules.c src/game/extra_state.c src/game/undo.c src/game/file.c src/game/file_io.c"

FORBIDDEN='\b(float|double)\b|\b(s?rand|time|clock)[[:space:]]*\(|SDL_GetTicks|time_get_millis|#include <math\.h>'

# Known exceptions, each with its reason:
# - city/warning.c, city/message.c: user interface timing (warnings and popups), not simulation state
# - building/construction.c road_last_update: gatehouse orientation toggles while placing it,
#   to become a command parameter (ROADMAP M2.2)
# - mp/discovery.c: announcing and finding games on the network before a game starts, not simulation state
ALLOWED='^src/city/warning\.c:|^src/city/message\.c:|^src/building/construction\.c:[0-9]+:.*road_last_update|^src/mp/discovery\.c:'

# shellcheck disable=SC2086
violations=$(grep -rnE "$FORBIDDEN" $SIMULATION | grep -vE "$ALLOWED" | grep -vE '^[^:]+:[0-9]+:[[:space:]]*(//|\*)' || true)
if [ -n "$violations" ]; then
    echo "ERROR: non-deterministic code in the simulation (floating point, clock or rand):" >&2
    echo "$violations" >&2
    exit 1
fi
echo "Determinism check passed"
