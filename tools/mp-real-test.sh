#!/usr/bin/env bash
# Two instances of the real game (no window), one hosting and one joining a network game, each
# driven by an automation script. Fails if the game desynchronises or does not run.
# Usage: tools/mp-real-test.sh [SAVE]   (SAVE relative to the repository, default test/data/request_start.sav)
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
SAVE=${1:-test/data/request_start.sav}
PORT=27450
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
tools/run-automation.sh test/automation/mp-host.txt 180 --mp-host "$SAVE" --mp-players 2 --mp-port $PORT \
    > build/automation/mp-host.log 2>&1 &
HOST=$!
sleep 1
tools/run-automation.sh test/automation/mp-client.txt 180 --mp-join 127.0.0.1:$PORT \
    > build/automation/mp-client.log 2>&1
CLIENT_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "Multiplayer:|mpcheck|automation\] .*(fail|timed|stopped)" build/automation/mp-host.log build/automation/mp-client.log \
    | sed -E 's/^.*(INFO|ERROR): //'
# the game writes its session files in the data directory: remove them
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, client $CLIENT_STATUS), logs in build/automation/mp-*.log"
    exit 1
fi
echo "OK: the network game ran without desynchronisation"
