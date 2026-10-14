#!/usr/bin/env bash
# Two instances of the real game (no window) play a network game of two players from the lobby (M10.8, D-088). The host
# loads, from the main menu, a game written by "simtool battlesave": the players are at war, the legion of player 1
# fights the legion of player 2 in his land, both have lost soldiers. The military advisor of each tells his war, the
# enemy soldiers in his city and the losses of both sides; both games stay in step (mpcheck).
# Screenshots in build/automation/battle-*.png.
# Usage: tools/serial.sh tools/mp-battle-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
SAVE="$DATA_DIR/essai battle.mpsav"
rm -f "$SAVE"
if ! (cd build/test && ./simtool battlesave brugle-massilia-start.sav "$SAVE" > ../automation/mp-battle-save.log 2>&1); then
    cat build/automation/mp-battle-save.log
    echo "FAILED: the game of the test could not be written"
    mp_restore_autosave
    exit 1
fi
tools/run-automation.sh test/automation/mp-battle-host.txt 300 > build/automation/mp-battle-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh test/automation/mp-battle-client.txt 300 > build/automation/mp-battle-client.log 2>&1
CLIENT_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "mpcheck|lobby|automation\] .*(fail|timed|stopped)" build/automation/mp-battle-host.log \
    build/automation/mp-battle-client.log | sed -E 's/^.*(INFO|ERROR): //'
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-* "$SAVE"
mp_restore_autosave
MISSING=0
for shot in host-advisor client-advisor; do
    [ -s "build/automation/battle-$shot.png" ] || { echo "missing screenshot battle-$shot.png"; MISSING=1; }
done
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ] || [ $MISSING -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, client $CLIENT_STATUS), logs in build/automation/mp-battle-*.log"
    exit 1
fi
echo "OK: the military advisors of a battle between two players, screenshots in build/automation/battle-*.png"
