#!/usr/bin/env bash
# Two instances of the real game (no window) play a network game from the lobby, save it by the File menu (a
# .mpsav), then desynchronise on purpose (T5.3). Both go back to the main menu by File, New game; the host
# loads the saved game by the Load menu, which opens the lobby with that game chosen (T5.4), the lobby also lists the
# monthly saved game, and the host hosts the saved game; the client joins again from the lobby; the resumed game must
# run without desynchronisation.
# Usage: tools/mp-resume-test.sh
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
. tools/mp-autosave-guard.sh
mp_keep_autosave
mkdir -p build/automation
rm -f "$DATA_DIR/essai 710.mpsav"
tools/run-automation.sh test/automation/mp-resume-host.txt 300 > build/automation/mp-resume-host.log 2>&1 &
HOST=$!
sleep 2
tools/run-automation.sh test/automation/mp-resume-client.txt 300 > build/automation/mp-resume-client.log 2>&1
CLIENT_STATUS=$?
wait $HOST
HOST_STATUS=$?
grep -h -E "Multiplayer:|mpcheck|mpwaitdesync|lobby|automation\] .*(fail|timed|stopped|log:)" \
    build/automation/mp-resume-host.log build/automation/mp-resume-client.log | sed -E 's/^.*(INFO|ERROR): //'
SAVED=0
[ -f "$DATA_DIR/essai 710.mpsav" ] && SAVED=1
rm -f "$DATA_DIR"/mp-session-* "$DATA_DIR"/mp-desync-* "$DATA_DIR/essai 710.mpsav"
mp_restore_autosave
if [ $HOST_STATUS -ne 0 ] || [ $CLIENT_STATUS -ne 0 ] || [ $SAVED -ne 1 ]; then
    echo "FAILED (host $HOST_STATUS, client $CLIENT_STATUS, saved $SAVED), logs in build/automation/mp-resume-*.log"
    exit 1
fi
echo "OK: a game saved by the File menu and out of sync was resumed from the lobby"
