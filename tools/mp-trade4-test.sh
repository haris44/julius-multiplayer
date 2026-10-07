#!/usr/bin/env bash
# Four instances of the real game (no window) start a network game of four players from the lobby; the host opens the
# trade advisor, whose tabs (empire, three players, stocks) must fit side by side, raises a price to four digits and
# takes screenshots (build/automation/trade4-*.png): nothing may overlap (T5.6). The language is the one of the game
# data.
# Usage: tools/mp-trade4-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build/automation
# "# REPEAT N command" becomes N times "command", three frames apart
awk '/^# REPEAT / { n = $3; cmd = $0; sub(/^# REPEAT [0-9]+ /, "", cmd); for (i = 0; i < n; i++) { print cmd; print "wait 3" } next } { print }' \
    test/automation/mp-trade4-host.txt > build/automation/mp-trade4-host.txt
tools/run-automation.sh build/automation/mp-trade4-host.txt 240 > build/automation/mp-trade4-host.log 2>&1 &
HOST=$!
sleep 2
CLIENTS=""
for i in 1 2 3; do
    tools/run-automation.sh test/automation/mp-trade4-client.txt 240 > build/automation/mp-trade4-client$i.log 2>&1 &
    CLIENTS="$CLIENTS $!"
    sleep 1
done
wait $HOST
HOST_STATUS=$?
CLIENT_STATUS=0
for pid in $CLIENTS; do
    wait "$pid" || CLIENT_STATUS=1
done
grep -h -E "mpcheck|automation\] .*(fail|timed|stopped)" build/automation/mp-trade4-host.log \
    build/automation/mp-trade4-client*.log | sed -E 's/^.*(INFO|ERROR): //'
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
rm -f build/automation/mp-trade4-host.txt
MISSING=0
for shot in player big-price empire stocks; do
    [ -s "build/automation/trade4-$shot.png" ] || { echo "missing screenshot trade4-$shot.png"; MISSING=1; }
done
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ] || [ $MISSING -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, clients $CLIENT_STATUS), logs in build/automation/mp-trade4-*.log"
    exit 1
fi
echo "OK: trade window of four players, screenshots in build/automation/trade4-*.png"
