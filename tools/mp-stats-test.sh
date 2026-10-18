#!/usr/bin/env bash
# The real game (no window): the statistics of Caesar (D-091). First a developed city under the rules of multiplayer,
# not networked (test/automation/caesar-stats.txt): the imperial advisor, its second page and the ratings with the
# laurels pillar at 640 x 480, 800 x 600 and 1024 x 768, before and after Caesar's wrath reaches the ultimatum. Then a
# game alone from the lobby (test/automation/mp-stats-solo.txt): the banner at the three sizes, with the wrath, and the
# imperial advisor. Screenshots in build/automation/stats-*.png; four players: tools/mp-stats4-test.sh.
# Usage: tools/serial.sh tools/mp-stats-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
tools/run-automation.sh test/automation/caesar-stats.txt 240 > build/automation/mp-stats-city.log 2>&1
CITY_STATUS=$?
tools/run-automation.sh test/automation/mp-stats-solo.txt 240 > build/automation/mp-stats-solo.log 2>&1
SOLO_STATUS=$?
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
MISSING=0
for shot in 640-imperial 640-details 640-ratings 640-laurels 640-imperial-wrath 640-details-wrath 1024-imperial \
    1024-details 1024-laurels 800-imperial solo-800-banner solo-800-wrath solo-640-banner solo-640-imperial \
    solo-640-details solo-1024-banner solo-1024-imperial solo-1024-details; do
    [ -s "build/automation/stats-$shot.png" ] || { echo "missing screenshot stats-$shot.png"; MISSING=1; }
done
if [ $CITY_STATUS -ne 0 ] || [ $SOLO_STATUS -ne 0 ] || [ $MISSING -ne 0 ]; then
    echo "FAILED (city $CITY_STATUS, alone $SOLO_STATUS), logs in build/automation/mp-stats-*.log"
    exit 1
fi
echo "OK: the statistics of Caesar, screenshots in build/automation/stats-*.png"
