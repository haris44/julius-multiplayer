#!/usr/bin/env bash
# Three instances of the real game (no window) start a network game of three players from the lobby (T5.7). On the
# trade page, by clicks only, each player proposes a route to the two others and buys marble from the first and iron
# from the second. Checks that the six routes open and that, at the first monthly departure, every computer looked at
# the six purchases (each of them in its log: nobody has goods to sell yet), without desynchronisation; the pages, with
# the reason in the "On the way" column, in build/automation/trade3-*.png.
# Usage: tools/serial.sh tools/mp-trade3-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build/automation
sed 's/CLIENT/client1/g' test/automation/mp-trade3-client.txt > build/automation/mp-trade3-client1.txt
sed 's/CLIENT/client2/g' test/automation/mp-trade3-client.txt > build/automation/mp-trade3-client2.txt
tools/run-automation.sh test/automation/mp-trade3-host.txt 300 > build/automation/mp-trade3-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh build/automation/mp-trade3-client1.txt 300 > build/automation/mp-trade3-client1.log 2>&1 &
CLIENT1=$!
sleep 3
tools/run-automation.sh build/automation/mp-trade3-client2.txt 300 > build/automation/mp-trade3-client2.log 2>&1
CLIENT2_STATUS=$?
wait $CLIENT1
CLIENT1_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "mpcheck|automation\] .*(fail|timed|stopped)" build/automation/mp-trade3-host.log \
    build/automation/mp-trade3-client1.log build/automation/mp-trade3-client2.log | sed -E 's/^.*(INFO|ERROR): //'
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-* build/automation/mp-trade3-client?.txt
# every computer looked at the six purchases of the three players at the first departure of the month: the same lines
FAILED=0
for log in host client1 client2; do
    lines=$(grep -c "Trade between players:.*the seller has none to sell" "build/automation/mp-trade3-$log.log")
    pairs=$(grep -o -E "Trade between players: +player [0-9] to player [0-9]" "build/automation/mp-trade3-$log.log" |
        sed -E "s/: +/: /" | sort -u | wc -l | tr -d ' ')
    echo "$log: $lines purchases looked at, between $pairs pairs of players"
    [ "$pairs" -eq 6 ] || FAILED=1
done
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT1_STATUS -ne 0 ] || [ $CLIENT2_STATUS -ne 0 ] || [ $FAILED -ne 0 ] \
    || [ ! -s build/automation/trade3-host-reasons.png ]; then
    echo "FAILED (host $HOST_STATUS, clients $CLIENT1_STATUS $CLIENT2_STATUS), logs in build/automation/mp-trade3-*.log"
    exit 1
fi
echo "OK: three players open their routes and buy from each other by the trade page, screenshots in build/automation/trade3-*.png"
