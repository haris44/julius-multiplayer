#!/usr/bin/env bash
# Two instances of the real game (no window) start a network game from the lobby; the host then opens the trade
# advisor, where the empire and the players share one page, and takes a screenshot (build/automation/trade-*.png).
# The client buys marble from the host, who then raises its price: the client sees the full-screen alert
# (build/automation/price-alert*.png). The host then sets a stock limit on the stock tab (trade-stocks.png, T4.5).
# Usage: tools/mp-trade-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
tools/run-automation.sh test/automation/mp-trade-host.txt 180 > build/automation/mp-trade-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh test/automation/mp-trade-client.txt 180 > build/automation/mp-trade-client.log 2>&1
CLIENT_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "mpcheck|tradecheck|automation\] .*(fail|timed|stopped)" build/automation/mp-trade-host.log \
    build/automation/mp-trade-client.log | sed -E 's/^.*(INFO|ERROR): //'
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ] || [ ! -s build/automation/trade-window.png ] \
    || [ ! -s build/automation/price-alert.png ]; then
    echo "FAILED (host $HOST_STATUS, client $CLIENT_STATUS), logs in build/automation/mp-trade-*.log"
    exit 1
fi
echo "OK: trade window and price alert in a network game, screenshots in build/automation/trade-*.png, price-alert*.png"
