#!/usr/bin/env bash
# Two instances of the real game (no window) start a network game from the lobby, the host at 640 x 480 and the
# client at 1024 x 768, and take screenshots of the windows changed by T4 (build/automation/ui-*.png): the lobby of
# the client with the rules of the host, the build menus of each place (farms, raw materials), the Options menu, the
# imperial advisor, the ratings with the laurels pillar, the trade advisor and its stock tab.
# Usage: tools/mp-ui-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
tools/run-automation.sh test/automation/mp-ui-host.txt 240 > build/automation/mp-ui-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh test/automation/mp-ui-client.txt 240 > build/automation/mp-ui-client.log 2>&1
CLIENT_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "mpcheck|automation\] .*(fail|timed|stopped)" build/automation/mp-ui-host.log \
    build/automation/mp-ui-client.log | sed -E 's/^.*(INFO|ERROR): //'
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-*
mp_restore_autosave
MISSING=0
for shot in host-lobby host-city host-farms host-raw host-imperial host-ratings host-stocks \
    client-lobby client-farms client-raw client-options client-ratings client-trade; do
    [ -s "build/automation/ui-$shot.png" ] || { echo "missing screenshot ui-$shot.png"; MISSING=1; }
done
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ] || [ $MISSING -ne 0 ]; then
    echo "FAILED (host $HOST_STATUS, client $CLIENT_STATUS), logs in build/automation/mp-ui-*.log"
    exit 1
fi
echo "OK: screenshots of the lobby, build menus, advisors and trade in build/automation/ui-*.png"
