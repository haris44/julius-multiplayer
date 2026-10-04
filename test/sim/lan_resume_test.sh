#!/bin/sh
# A network game that goes on from a multiplayer saved game: 3 cities written by simtool, then hosted for
# 3 players; every player must end with the same checksum.
# Usage: lan_resume_test.sh SIMTOOL PORT
SIMTOOL=$1; PORT=$2
SAVE="lan-resume-$PORT.mpsav"
"$SIMTOOL" mpsave brugle-lugdunum.sav 3 300 "$SAVE" || exit 1
sh "$(dirname "$0")/lan_test.sh" "$SIMTOOL" "$PORT" 3 "$SAVE" 400
STATUS=$?
rm -f "$SAVE"
exit $STATUS
