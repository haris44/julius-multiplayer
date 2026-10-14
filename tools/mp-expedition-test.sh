#!/usr/bin/env bash
# The real game (no window): Caesar's punitive expedition in a game alone from the lobby (M10.6, D-086): the lobby
# option of the second expedition, the letter of the expedition, Caesar's legions in the city, the imperial advisor
# in disgrace, the letters of an expedition destroyed and gone, the end screen when Rome takes the province back
# (test/automation/caesar-expedition.txt). Screenshots in build/automation/expedition-*.png.
# Usage: tools/serial.sh tools/mp-expedition-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
tools/run-automation.sh test/automation/caesar-expedition.txt 300 > build/automation/mp-expedition.log 2>&1
STATUS=$?
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
MISSING=0
for shot in lobby message letter legions imperial defeated left rome; do
    [ -s "build/automation/expedition-$shot.png" ] || { echo "missing screenshot expedition-$shot.png"; MISSING=1; }
done
if [ $STATUS -ne 0 ] || [ $MISSING -ne 0 ]; then
    echo "FAILED, log in build/automation/mp-expedition.log"
    exit 1
fi
echo "OK: Caesar's punitive expedition, screenshots in build/automation/expedition-*.png"
