#!/usr/bin/env bash
# Two instances of the real game (no window) play a network game of two players at war from the lobby (M10.2, D-087).
# The host loads, from the main menu, a game written by "simtool attacksave": player 1 fights player 2, his legion of
# 16 legionaries rests in its fort, player 2 has a quarter of statues, a well and prefectures. The host opens the legion
# window (right click on the soldiers), clicks "Attack", then a tile of the quarter of player 2: the legion marches
# there and pulls the quarter down. Both games must stay in step (mpcheck). Screenshots in
# build/automation/attack-*.png: the legion window with the button, the pick of the target, the legion at work and its
# window while it attacks.
# Usage: tools/serial.sh tools/mp-attack-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
SAVE="$DATA_DIR/essai attack.mpsav"
rm -f "$SAVE"
if ! (cd build/test && ./simtool attacksave brugle-massilia-start.sav "$SAVE" > ../automation/mp-attack-save.log 2>&1); then
    cat build/automation/mp-attack-save.log
    echo "FAILED: the game of the test could not be written"
    mp_restore_autosave
    exit 1
fi
tools/run-automation.sh test/automation/mp-attack-host.txt 600 > build/automation/mp-attack-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh test/automation/mp-attack-client.txt 600 > build/automation/mp-attack-client.log 2>&1
CLIENT_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "mpcheck|lobby|automation\] .*(fail|timed|stopped)" build/automation/mp-attack-host.log \
    build/automation/mp-attack-client.log | sed -E 's/^.*(INFO|ERROR): //'
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-* "$SAVE"
mp_restore_autosave
MISSING=0
for shot in host-legion host-pick host-attacking host-legion-attacking host-done client-city; do
    [ -s "build/automation/attack-$shot.png" ] || { echo "missing screenshot attack-$shot.png"; MISSING=1; }
done
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ] || [ $MISSING -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, client $CLIENT_STATUS), logs in build/automation/mp-attack-*.log"
    exit 1
fi
echo "OK: the legion of player 1 attacks the quarter of player 2 in a network game, screenshots in" \
    "build/automation/attack-*.png"
