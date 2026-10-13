#!/usr/bin/env bash
# The real game (no window): the windows of the barracks and of the forts in a multiplayer game alone (MG.4, D-083).
# Usage: tools/serial.sh tools/mg-barracks-test.sh      (captures build/automation/mg-*.png, to be looked at)
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
tools/run-automation.sh test/automation/mg-barracks.txt 240 > build/automation/mg.log 2>&1
STATUS=$?
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
if [ $STATUS -ne 0 ]; then
    echo "FAILED, log in build/automation/mg.log"
    exit 1
fi
echo "OK: captures in build/automation/mg-*.png"
