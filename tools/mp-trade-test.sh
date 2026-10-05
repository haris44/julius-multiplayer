#!/usr/bin/env bash
# Two instances of the real game (no window) start a network game from the lobby; the host then opens the trade
# advisor and the window of trade between players, and takes screenshots of both (build/automation/trade-*.png).
# Usage: tools/mp-trade-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build/automation
tools/run-automation.sh test/automation/mp-trade-host.txt 180 > build/automation/mp-trade-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh test/automation/mp-lobby-client.txt 180 > build/automation/mp-trade-client.log 2>&1
CLIENT_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "mpcheck|automation\] .*(fail|timed|stopped)" build/automation/mp-trade-host.log \
    build/automation/mp-trade-client.log | sed -E 's/^.*(INFO|ERROR): //'
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ] || [ ! -s build/automation/trade-window.png ]; then
    echo "FAILED (host $HOST_STATUS, client $CLIENT_STATUS), logs in build/automation/mp-trade-*.log"
    exit 1
fi
echo "OK: the trade window opened in a network game, screenshots in build/automation/trade-*.png"
