#!/usr/bin/env bash
# Three instances of the real game (no window) play a network game of 3 cities copied from a developed saved game;
# each opens and draws every window, advisor and information panel while the game runs (automation "windowsweep",
# T5.3): no window may change the simulated state, and the game must go on without desynchronisation.
# Usage: tools/mp-menus-test.sh [SAVE]   (SAVE relative to the repository, default test/data/brugle-lugdunum.sav)
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SAVE=${1:-test/data/brugle-lugdunum.sav}
PORT=27520
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
tools/run-automation.sh test/automation/mp-menus-host.txt 300 --mp-host "$SAVE" --mp-players 3 --mp-port $PORT \
    > build/automation/mp-menus-host.log 2>&1 &
HOST=$!
sleep 1
tools/run-automation.sh test/automation/mp-menus-client.txt 300 --mp-join 127.0.0.1:$PORT \
    > build/automation/mp-menus-client1.log 2>&1 &
CLIENT1=$!
tools/run-automation.sh test/automation/mp-menus-client.txt 300 --mp-join 127.0.0.1:$PORT \
    > build/automation/mp-menus-client2.log 2>&1
CLIENT2_STATUS=$?
wait $CLIENT1
CLIENT1_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "Multiplayer:|windowsweep|mpcheck|automation\] .*(fail|timed|stopped|log:)" build/automation/mp-menus-*.log \
    | sed -E 's/^.*(INFO|ERROR): //'
# the game writes its session files in the data directory: remove them
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT1_STATUS -ne 0 ] || [ $CLIENT2_STATUS -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, clients $CLIENT1_STATUS and $CLIENT2_STATUS), logs in build/automation/mp-menus-*.log"
    exit 1
fi
echo "OK: three players opened every window while the game ran, without desynchronisation"
