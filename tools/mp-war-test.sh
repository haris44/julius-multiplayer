#!/usr/bin/env bash
# Two instances of the real game (no window) start a network game from the lobby: the host (640 x 480) declares a
# brutal war on the client (1024 x 768) from the military advisor; the client sees it announced, proposes peace, the
# host accepts it (T5.5). Screenshots in build/automation/war-*.png; both games must stay in step (mpcheck).
# Usage: tools/serial.sh tools/mp-war-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build/automation
tools/run-automation.sh test/automation/mp-war-host.txt 240 > build/automation/mp-war-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh test/automation/mp-war-client.txt 240 > build/automation/mp-war-client.log 2>&1
CLIENT_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "mpcheck|automation\] .*(fail|timed|stopped)" build/automation/mp-war-host.log \
    build/automation/mp-war-client.log | sed -E 's/^.*(INFO|ERROR): //'
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
MISSING=0
for shot in host-advisor host-peace host-confirm host-war host-offer host-signed client-city client-war client-offer \
    client-signed; do
    [ -s "build/automation/war-$shot.png" ] || { echo "missing screenshot war-$shot.png"; MISSING=1; }
done
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ] || [ $MISSING -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, client $CLIENT_STATUS), logs in build/automation/mp-war-*.log"
    exit 1
fi
echo "OK: war declared and peace signed in a network game, screenshots in build/automation/war-*.png"
