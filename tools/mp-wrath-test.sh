#!/usr/bin/env bash
# The real game (no window): Caesar's wrath in a game alone from the lobby (M10.5): letters of the warning, of the
# ultimatum and of the peace imposed, the gauge in the banner, the imperial advisor (test/automation/caesar-wrath.txt).
# Screenshots in build/automation/wrath-*.png.
# Usage: tools/serial.sh tools/mp-wrath-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
tools/run-automation.sh test/automation/caesar-wrath.txt 240 > build/automation/mp-wrath.log 2>&1
STATUS=$?
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
MISSING=0
for shot in warning ultimatum banner imperial peace; do
    [ -s "build/automation/wrath-$shot.png" ] || { echo "missing screenshot wrath-$shot.png"; MISSING=1; }
done
if [ $STATUS -ne 0 ] || [ $MISSING -ne 0 ]; then
    echo "FAILED, log in build/automation/mp-wrath.log"
    exit 1
fi
echo "OK: the letters and the gauge of Caesar's wrath, screenshots in build/automation/wrath-*.png"
