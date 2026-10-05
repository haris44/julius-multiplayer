#!/usr/bin/env bash
# Two instances of the real game (no window) that start a network game through the lobby only:
# the host picks a map and hosts, the client finds the game on the network and joins it.
# The lobby only offers multiplayer maps: the host's map is a large generated one (D-033).
# Usage: tools/mp-lobby-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p build/automation
tools/run-automation.sh test/automation/mp-lobby-host.txt 180 > build/automation/mp-lobby-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh test/automation/mp-lobby-client.txt 180 > build/automation/mp-lobby-client.log 2>&1
CLIENT_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "Multiplayer:|mpcheck|automation\] .*(fail|timed|stopped)" build/automation/mp-lobby-host.log \
    build/automation/mp-lobby-client.log | sed -E 's/^.*(INFO|ERROR): //'
# the game writes its session files in the data directory: remove them
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, client $CLIENT_STATUS), logs in build/automation/mp-lobby-*.log"
    exit 1
fi
echo "OK: the network game started from the lobby ran without desynchronisation"
