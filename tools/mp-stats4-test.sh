#!/usr/bin/env bash
# Four instances of the real game (no window) start a network game of four players from the lobby: the host at
# 640 x 480, a client at 1024 x 768 and two at 800 x 600. Two months later, each takes screenshots of the statistics
# of Caesar with four players (build/automation/stats4-*.png, D-091): the banner of the laurels at the bottom of the
# city, the imperial advisor and its ranking, the ratings with the laurels pillar. Nothing may overlap nor run under
# the sidebar.
# Usage: tools/serial.sh tools/mp-stats4-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build/automation
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
# the fourth player plays as the third one, without the screenshots
grep -v '^screenshot' test/automation/mp-stats4-client.txt > build/automation/mp-stats4-client-quiet.txt
tools/run-automation.sh test/automation/mp-stats4-host.txt 300 > build/automation/mp-stats4-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh test/automation/mp-stats4-client1024.txt 300 > build/automation/mp-stats4-client1.log 2>&1 &
CLIENTS="$!"
sleep 1
tools/run-automation.sh test/automation/mp-stats4-client.txt 300 > build/automation/mp-stats4-client2.log 2>&1 &
CLIENTS="$CLIENTS $!"
sleep 1
tools/run-automation.sh build/automation/mp-stats4-client-quiet.txt 300 > build/automation/mp-stats4-client3.log 2>&1 &
CLIENTS="$CLIENTS $!"
wait $HOST
HOST_STATUS=$?
CLIENT_STATUS=0
for pid in $CLIENTS; do
    wait "$pid" || CLIENT_STATUS=1
done
grep -h -E "mpcheck|automation\] .*(fail|timed|stopped)" build/automation/mp-stats4-host.log \
    build/automation/mp-stats4-client*.log | sed -E 's/^.*(INFO|ERROR): //'
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
rm -f build/automation/mp-stats4-client-quiet.txt
MISSING=0
for shot in 640-city 640-imperial 640-laurels 1024-city 1024-imperial 1024-laurels 800-city 800-imperial; do
    [ -s "build/automation/stats4-$shot.png" ] || { echo "missing screenshot stats4-$shot.png"; MISSING=1; }
done
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ] || [ $MISSING -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, clients $CLIENT_STATUS), logs in build/automation/mp-stats4-*.log"
    exit 1
fi
echo "OK: the statistics of Caesar with four players, screenshots in build/automation/stats4-*.png"
