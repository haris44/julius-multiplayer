#!/usr/bin/env bash
# The real game (no window) plays a network game alone, with the city of a developed saved game, and opens and draws
# every window, advisor and information panel without a tick ("windowsweep", T5.3): none may change the simulated
# state, or the players of a network game would desynchronise. Then tools/mp-menus-test.sh does the same at three.
# Usage: tools/mp-sweep-test.sh [SAVE]   (SAVE relative to the repository, default test/data/brugle-lugdunum.sav;
#        MP_SWEEP_ARGS=--mp-generate: the prepared map, SAVE giving climate and empire)
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SAVE=${1:-test/data/brugle-lugdunum.sav}
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
tools/run-automation.sh test/automation/mp-sweep-solo.txt 300 --mp-host "$SAVE" --mp-players 1 --mp-port 27521 ${MP_SWEEP_ARGS:-} \
    > build/automation/mp-sweep.log 2>&1
STATUS=$?
grep -h -E "windowsweep|angrygod|automation\] .*(fail|timed|stopped)" build/automation/mp-sweep.log \
    | sed -E 's/^.*(INFO|ERROR): //'
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
if [ $STATUS -ne 0 ]; then
    echo "FAILED ($STATUS), log in build/automation/mp-sweep.log"
    exit 1
fi
echo "OK: no window changed the simulated state"
