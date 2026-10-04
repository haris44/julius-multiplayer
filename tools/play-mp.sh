#!/usr/bin/env bash
# Starts a network game on this Mac, one real game window per player, to play and test by hand.
#
# Usage: tools/play-mp.sh [PLAYERS] [SAVE]
#   PLAYERS  number of windows/players, 2 to 4 (default 2)
#   SAVE     saved game to start from (default test/data/request_start.sav)
#
# Playing on two Macs instead: on the host Mac
#   build/julius.app/Contents/MacOS/julius --mp-host SAVE --mp-players 2 ../donnees-c3
# and on the other Mac (host address: `ipconfig getifaddr en0` on the host)
#   build/julius.app/Contents/MacOS/julius --mp-join HOST_ADDRESS ../donnees-c3
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PLAYERS=${1:-2}
SAVE=${2:-$ROOT/test/data/request_start.sav}
DATA_DIR=${C3_DATA_DIR:-$ROOT/../donnees-c3}
BIN="$ROOT/build/julius.app/Contents/MacOS/julius"
[ -x "$BIN" ] || { echo "Build the game first: tools/check.sh" >&2; exit 2; }

"$BIN" --windowed --mp-host "$SAVE" --mp-players "$PLAYERS" "$DATA_DIR" > "$ROOT/build/mp-player1.log" 2>&1 &
sleep 1
for ((i = 2; i <= PLAYERS; i++)); do
    "$BIN" --windowed --mp-join 127.0.0.1 "$DATA_DIR" > "$ROOT/build/mp-player$i.log" 2>&1 &
    sleep 0.5
done
echo "$PLAYERS game windows started (logs: build/mp-player*.log)."
echo "The game starts when every player has joined. The black banner at the top left shows your player number."
wait
