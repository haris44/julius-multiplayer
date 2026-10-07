#!/usr/bin/env bash
# Three instances of the real game (no window) play a network game of three players from the lobby (T5.7, T5.6). The
# host loads, from the main menu, a game written by "simtool tradesave": each player has a warehouse at the end of a
# road of his own joined to the main road, player 2 has marble and iron to sell. On the trade page, by clicks only,
# each player proposes a route to the two others and buys marble from the first and iron from the second. Checks,
# without desynchronisation and in the log of every computer:
# - the six purchases are looked at at the first monthly departure (the six pairs of players);
# - caravans of player 2 leave, and the marble really arrives at player 1 and the iron at player 3, paid;
# - the other purchases tell why nothing leaves (the seller has none to sell).
# The pages in build/automation/trade3-*.png: purchases asked, marble on the way, delivered, reasons.
# Usage: tools/serial.sh tools/mp-trade3-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
SAVE="$DATA_DIR/essai trade3.mpsav"
rm -f "$SAVE"
if ! (cd build/test && ./simtool tradesave brugle-massilia-start.sav "$SAVE" > ../automation/mp-trade3-save.log 2>&1); then
    cat build/automation/mp-trade3-save.log
    echo "FAILED: the game of the test could not be written"
    mp_restore_autosave
    exit 1
fi
sed 's/CLIENT/client1/g' test/automation/mp-trade3-client.txt > build/automation/mp-trade3-client1.txt
sed 's/CLIENT/client2/g' test/automation/mp-trade3-client.txt > build/automation/mp-trade3-client2.txt
tools/run-automation.sh test/automation/mp-trade3-host.txt 900 > build/automation/mp-trade3-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh build/automation/mp-trade3-client1.txt 900 > build/automation/mp-trade3-client1.log 2>&1 &
CLIENT1=$!
sleep 3
tools/run-automation.sh build/automation/mp-trade3-client2.txt 900 > build/automation/mp-trade3-client2.log 2>&1
CLIENT2_STATUS=$?
wait $CLIENT1
CLIENT1_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "mpcheck|lobby|automation\] .*(fail|timed|stopped)" build/automation/mp-trade3-host.log \
    build/automation/mp-trade3-client1.log build/automation/mp-trade3-client2.log | sed -E 's/^.*(INFO|ERROR): //'
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-* "$SAVE" build/automation/mp-trade3-client?.txt
mp_restore_autosave
FAILED=0
for log in host client1 client2; do
    file="build/automation/mp-trade3-$log.log"
    none=$(grep -c "Trade between players:.*the seller has none to sell" "$file")
    pairs=$(grep -o -E "Trade between players: +player [0-9] to player [0-9]" "$file" |
        sed -E "s/: +/: /" | sort -u | wc -l | tr -d ' ')
    left=$(grep -c -E "Trade between players:.*player 2 to player [13], resource [0-9]+: a caravan leaves" "$file")
    marble=$(grep -E "Trade between players:.*player 2 to player 1, resource 12: [1-9][0-9]* of [0-9]+ loads delivered" \
        "$file" | head -1 | sed -E 's/^.*resource 12: //')
    iron=$(grep -E "Trade between players:.*player 2 to player 3, resource 9: [1-9][0-9]* of [0-9]+ loads delivered" \
        "$file" | head -1 | sed -E 's/^.*resource 9: //')
    echo "$log: purchases between $pairs pairs of players, $none times nothing to sell, $left caravans left;" \
        "marble to player 1: ${marble:-none}; iron to player 3: ${iron:-none}"
    [ "$pairs" -eq 6 ] && [ "$none" -gt 0 ] && [ "$left" -ge 2 ] && [ -n "$marble" ] && [ -n "$iron" ] || FAILED=1
done
for shot in host-asked host-on-the-way host-delivered host-reasons client1-reasons client2-reasons; do
    [ -s "build/automation/trade3-$shot.png" ] || { echo "missing screenshot trade3-$shot.png"; FAILED=1; }
done
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT1_STATUS -ne 0 ] || [ $CLIENT2_STATUS -ne 0 ] || [ $FAILED -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, clients $CLIENT1_STATUS $CLIENT2_STATUS), logs in build/automation/mp-trade3-*.log"
    exit 1
fi
echo "OK: three players trade by the trade page, caravans of player 2 arrive at players 1 and 3," \
    "screenshots in build/automation/trade3-*.png"
