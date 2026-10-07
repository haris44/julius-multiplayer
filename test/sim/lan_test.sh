#!/bin/sh
# Network game test: one host and N-1 clients, headless, on this computer.
# Usage: lan_test.sh SIMTOOL PORT PLAYERS SAVE TICKS [desync] [cities]
# Without 'desync': every player must end with the same checksum and the host must verify every turn.
# With 'desync': the last client changes its own state; host and client must detect it.
# With 'cities': every player has its own copy of the city (separate cities) instead of a shared city.
# With 'generate': every player starts an empty city on a large generated map (SAVE gives climate and empire).
# With 'pause': the last client pauses the game at half time and resumes it; everyone must see the pause.
# With 'leave': the last client leaves at half time; the others must finish the game together.
# With 'baddata': 2 players, the client pretends to have other game data; the host must refuse it.
# With 'rules': the host changes every rule of the lobby once the players are there, then starts the game; every
# player must play with the changed rules, which the clients saw in their lobby (T4.11).
# With 'map2' (and 'generate'): the host chooses the prepared map 2; every player must play on it (T4.14).
SIMTOOL=$1; PORT=$2; PLAYERS=$3; SAVE=$4; TICKS=$5; MODE=$6
CITIES=""
MAP2=""
for arg in "$@"; do
    [ "$arg" = "cities" ] && CITIES="cities"
    [ "$arg" = "generate" ] && CITIES="generate"
    [ "$arg" = "map2" ] && MAP2="map2"
done
[ "$MODE" = "cities" ] && MODE=""
[ "$MODE" = "generate" ] && MODE=""
[ "$MODE" = "map2" ] && MODE=""
DIR=$(mktemp -d)
HOST_ARGS=""
[ "$MODE" = "desync" ] && HOST_ARGS="expect-desync"
[ "$MODE" = "baddata" ] && HOST_ARGS="expect-reject"
[ "$MODE" = "rules" ] && HOST_ARGS="rules"
"$SIMTOOL" mpnode host "$PORT" "$PLAYERS" "$SAVE" "$TICKS" $CITIES $MAP2 $HOST_ARGS > "$DIR/host.log" 2>&1 &
HOST=$!
sleep 0.3
PIDS=""
i=1
while [ $i -lt "$PLAYERS" ]; do
    CLIENT_MODE=""
    [ "$MODE" = "desync" ] && CLIENT_MODE="expect-desync"
    [ "$MODE" = "desync" ] && [ $i -eq $((PLAYERS - 1)) ] && CLIENT_MODE="desync"
    [ "$MODE" = "pause" ] && [ $i -eq $((PLAYERS - 1)) ] && CLIENT_MODE="pause"
    [ "$MODE" = "leave" ] && [ $i -eq $((PLAYERS - 1)) ] && CLIENT_MODE="leave"
    [ "$MODE" = "baddata" ] && CLIENT_MODE="baddata"
    [ "$MODE" = "rules" ] && CLIENT_MODE="rules"
    "$SIMTOOL" mpnode join 127.0.0.1 "$PORT" "$TICKS" $CLIENT_MODE > "$DIR/client$i.log" 2>&1 &
    PIDS="$PIDS $!"
    i=$((i + 1))
done
FAILED=0
wait $HOST || FAILED=1
for pid in $PIDS; do
    wait "$pid" || FAILED=1
done
for f in "$DIR"/*.log; do
    echo "== $(basename "$f")"
    cat "$f"
done
if [ "$MODE" = "pause" ]; then
    SEEN=$(grep -h "pause seen: 1" "$DIR"/*.log | wc -l | tr -d ' ')
    [ "$SEEN" = "$PLAYERS" ] || { echo "The pause was not seen by every player"; FAILED=1; }
fi
if [ "$MODE" = "rules" ]; then
    SEEN=$(grep -h "^game rules:" "$DIR"/*.log | wc -l | tr -d ' ')
    DIFFERENT=$(grep -h "^game rules:" "$DIR"/*.log | sort -u | wc -l | tr -d ' ')
    [ "$SEEN" = "$PLAYERS" ] && [ "$DIFFERENT" = "1" ] || { echo "The players do not play with the same rules"; FAILED=1; }
    # every player, the clients having joined by address, saw the number of players of the host in his lobby (T4.11)
    SEEN=$(grep -h "^lobby players: $PLAYERS\$" "$DIR"/*.log | wc -l | tr -d ' ')
    [ "$SEEN" = "$PLAYERS" ] || { echo "The players do not all see $PLAYERS players in their lobby"; FAILED=1; }
fi
if [ -n "$MAP2" ]; then
    # every log must prove the map by its width: map 2 is 220 tiles wide for 2 players and 240 for 3 or 4
    # (map 1 would be 200 and 260)
    WIDTH=240
    [ "$PLAYERS" -le 2 ] && WIDTH=220
    SEEN=$(grep -h "^map 2, $WIDTH tiles wide\$" "$DIR"/*.log | wc -l | tr -d ' ')
    DIFFERENT=$(grep -h "^map " "$DIR"/*.log | sort -u | wc -l | tr -d ' ')
    [ "$SEEN" = "$PLAYERS" ] && [ "$DIFFERENT" = "1" ] || { echo "The players do not all play on map 2, $WIDTH tiles wide"; FAILED=1; }
fi
if [ "$MODE" != "desync" ] && [ "$MODE" != "baddata" ]; then
    COUNT=$(grep -h "checksum" "$DIR"/*.log | awk '{print $4}' | sort -u | wc -l | tr -d ' ')
    [ "$COUNT" = "1" ] || { echo "Final checksums differ"; FAILED=1; }
fi
rm -rf "$DIR" mp-session-"$PORT"-p*.sav mp-session-"$PORT"-p*.mpsav mp-desync-"$PORT"-*.sav mp-desync-"$PORT"-*.mpsav
exit $FAILED
