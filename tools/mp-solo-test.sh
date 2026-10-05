#!/usr/bin/env bash
# The real game (no window): a game alone started from the lobby on the prepared map. The player builds only in his
# zone, his first mission gives him one, and his missionary goes where he is sent with the mouse.
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
FAILED=0
# no house without a zone: only the mission and the house beside it
echo "$MISSION" | grep -q "buildings 2, zone [1-9]" || { echo "the mission did not give a zone, or a house was built outside"; FAILED=1; }
[ "$(echo "$START" | cut -d, -f1)" != "$(echo "$MOVED" | cut -d, -f1)" ] || { echo "the missionary did not move"; FAILED=1; }
if [ $STATUS -ne 0 ] || [ $FAILED -ne 0 ]; then
    echo "FAILED, log in build/automation/mp-solo.log"
    exit 1
fi
echo "OK: alone on the prepared map, zones and missionary work"
