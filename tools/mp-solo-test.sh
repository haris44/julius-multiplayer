#!/usr/bin/env bash
# The real game (no window): a game alone started from the lobby on the prepared map. The player starts with his
# mission and its zone, builds only in it, and his missionary goes where he is sent with the mouse, the bridge of
# Caesar included.
# Usage: tools/mp-solo-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build/automation
tools/run-automation.sh test/automation/mp-solo.txt 120 > build/automation/mp-solo.log 2>&1
STATUS=$?
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
INFO=$(grep -h "mpinfo:" build/automation/mp-solo.log | sed -E 's/^.*mpinfo: //')
echo "$INFO"
START=$(echo "$INFO" | sed -n 1p)
MISSION=$(echo "$INFO" | sed -n 2p)
MOVED=$(echo "$INFO" | sed -n 3p)
BRIDGE=$(echo "$INFO" | sed -n 4p)
FAILED=0
echo "$START" | grep -q "buildings 1, zone [1-9]" || { echo "no mission and zone at the start"; FAILED=1; }
# only the house in the zone: not the one outside, nor a mission without marble
echo "$MISSION" | grep -q "buildings 2, zone [1-9]" || { echo "a house was built outside the zone, or a mission without marble"; FAILED=1; }
[ "$(echo "$START" | cut -d, -f1)" != "$(echo "$MOVED" | cut -d, -f1)" ] || { echo "the missionary did not move"; FAILED=1; }
# the bridge of Caesar is column 100 of the map for 2
echo "$BRIDGE" | grep -q "missionary at (100, " ||
    { echo "a click on the bridge does not send the missionary onto it"; FAILED=1; }
if [ $STATUS -ne 0 ] || [ $FAILED -ne 0 ]; then
    echo "FAILED, log in build/automation/mp-solo.log"
    exit 1
fi
echo "OK: alone on the prepared map, the mission, the zone and the missionary work, over the bridge too"
