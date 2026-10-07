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
# With 'saveresume' (T5.4): the players also trade; at the end every player saves the game as the File menu does, the
# host hosts its saved game as the lobby does (same process), the other players join it again, except the last one,
# replaced by a new process; the resumed game must run TICKS more ticks with the same checksums everywhere.
# With 'dragmonth' (and 'saveresume'): the host drags a road while a month starts; the monthly saved game written then
# must not hold that road (T5.3 review).
# With 'difficulty-easy' or 'difficulty-hard' (and 'generate'; with 'map-template' the host's template is a free map made of SAVE, rank 5 and funds 3000):
# the host's lobby sets that difficulty while the settings of the computers are other ones; every city of every
# computer must start with the funds, favor, rank, salary and savings of that difficulty and of everybody (T5.1, T5.2).
# With 'war' (and 'cities'): player 1 declares a brutal war on player 2 and sends a legion to his city, then both sign
# peace; every player must see the same war, the same soldiers lost and the same checksum (T5.5).
SIMTOOL=$1; PORT=$2; PLAYERS=$3; SAVE=$4; TICKS=$5; MODE=$6
CITIES=""
MAP2=""
RESUME=""
DRAG=""
DIFF=""
MAPT=""
for arg in "$@"; do
    [ "$arg" = "map-template" ] && MAPT="map-template"
    case "$arg" in difficulty-easy|difficulty-hard) DIFF="$arg" ;; esac
    [ "$arg" = "cities" ] && CITIES="cities"
    [ "$arg" = "generate" ] && CITIES="generate"
    [ "$arg" = "map2" ] && MAP2="map2"
    [ "$arg" = "saveresume" ] && RESUME="saveresume"
    [ "$arg" = "dragmonth" ] && DRAG="dragmonth"
done
[ "$MODE" = "cities" ] && MODE=""
[ "$MODE" = "generate" ] && MODE=""
[ "$MODE" = "map2" ] && MODE=""
[ "$MODE" = "saveresume" ] && MODE=""
[ "$MODE" = "difficulty-easy" ] && MODE=""
[ "$MODE" = "difficulty-hard" ] && MODE=""
DIR=$(mktemp -d)
HOST_ARGS=""
[ "$MODE" = "desync" ] && HOST_ARGS="expect-desync"
[ "$MODE" = "baddata" ] && HOST_ARGS="expect-reject"
[ "$MODE" = "rules" ] && HOST_ARGS="rules"
[ "$MODE" = "war" ] && HOST_ARGS="war"
"$SIMTOOL" mpnode host "$PORT" "$PLAYERS" "$SAVE" "$TICKS" $CITIES $MAP2 $RESUME $DRAG $DIFF $MAPT $HOST_ARGS > "$DIR/host.log" 2>&1 &
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
    [ "$MODE" = "war" ] && CLIENT_MODE="war"
    # the last client plays only the first game: a new process replaces it in the resumed game
    [ -n "$RESUME" ] && [ $i -lt $((PLAYERS - 1)) ] && CLIENT_MODE="saveresume"
    "$SIMTOOL" mpnode join 127.0.0.1 "$PORT" "$TICKS" $CLIENT_MODE $DIFF $MAPT > "$DIR/client$i.log" 2>&1 &
    LAST=$!
    PIDS="$PIDS $LAST"
    i=$((i + 1))
done
LAST_FAILED=0
if [ -n "$RESUME" ]; then
    # the last client ends with the first game, then a new process joins the resumed game
    PIDS=$(echo "$PIDS" | sed "s/ $LAST\$//")
    wait $LAST || LAST_FAILED=1
    "$SIMTOOL" mpnode join 127.0.0.1 "$PORT" "$TICKS" > "$DIR/new-client.log" 2>&1 &
    PIDS="$PIDS $!"
fi
FAILED=$LAST_FAILED
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
if [ "$MODE" = "war" ]; then
    SEEN=$(grep -h "^war: " "$DIR"/*.log | wc -l | tr -d ' ')
    DIFFERENT=$(grep -h "^war: " "$DIR"/*.log | sort -u | wc -l | tr -d ' ')
    [ "$SEEN" = "$PLAYERS" ] && [ "$DIFFERENT" = "1" ] || { echo "The players did not all see the same war"; FAILED=1; }
fi
if [ -n "$MAP2" ]; then
    # every log must prove the map by its width: map 2 is 220 tiles wide for 2 players and 240 for 3 or 4
    # (map 1 would be 200 and 260)
    WIDTH=240
    [ "$PLAYERS" -le 2 ] && WIDTH=220
    SEEN=$(grep -h "^map 2, $WIDTH tiles wide\$" "$DIR"/*.log | wc -l | tr -d ' ')
    DIFFERENT=$(grep -h "^map " "$DIR"/*.log | sort -u | wc -l | tr -d ' ')
    # (a saved and resumed game prints its map twice)
    [ "$SEEN" -ge "$PLAYERS" ] && [ "$DIFFERENT" = "1" ] || { echo "The players do not all play on map 2, $WIDTH tiles wide"; FAILED=1; }
fi
if [ -n "$DIFF" ]; then
    # one line per city, the same on every computer; and the difficulty of the lobby everywhere
    EXPECTED=3
    [ "$DIFF" = "difficulty-easy" ] && EXPECTED=1
    SEEN=$(grep -h "^start city " "$DIR"/*.log | wc -l | tr -d ' ')
    DIFFERENT=$(grep -h "^start city " "$DIR"/*.log | sort -u | wc -l | tr -d ' ')
    [ "$SEEN" = "$((PLAYERS * PLAYERS))" ] && [ "$DIFFERENT" = "$PLAYERS" ] || { echo "The cities do not start the same on every computer"; FAILED=1; }
    SEEN=$(grep -h "^start difficulty $EXPECTED\$" "$DIR"/*.log | wc -l | tr -d ' ')
    [ "$SEEN" = "$PLAYERS" ] || { echo "The players do not all play at the difficulty of the lobby"; FAILED=1; }
fi
if [ -n "$RESUME" ]; then
    # the first game and the resumed one: each with the same final checksum for every player
    # (the new process plays only the resumed game)
    mv "$DIR/new-client.log" "$DIR/new-client.txt"
    COUNT=$(grep -h "^tick .* checksum" "$DIR"/*.log | grep -v resumed | awk '{print $4}' | sort -u | wc -l | tr -d ' ')
    [ "$COUNT" = "1" ] || { echo "Final checksums of the first game differ"; FAILED=1; }
    RESUMED=$( (grep -h "^tick .* checksum .* resumed" "$DIR"/*.log; grep -h "^tick .* checksum" "$DIR/new-client.txt") )
    COUNT=$(echo "$RESUMED" | awk '{print $4}' | sort -u | wc -l | tr -d ' ')
    SEEN=$(echo "$RESUMED" | grep -c checksum | tr -d ' ')
    [ "$COUNT" = "1" ] && [ "$SEEN" = "$PLAYERS" ] || { echo "The resumed game did not end the same for every player"; FAILED=1; }
    # the trade between players, as every player of the resumed game sees it at its end
    DIFFERENT=$(grep -h "^trade of city .* (end)" "$DIR"/host.log "$DIR"/new-client.txt | sort | uniq -c | awk '{print $1}' | sort -u | tr -d ' \n')
    [ "$DIFFERENT" = "2" ] || { echo "The players do not see the same trade"; FAILED=1; }
elif [ "$MODE" != "desync" ] && [ "$MODE" != "baddata" ]; then
    COUNT=$(grep -h "checksum" "$DIR"/*.log | awk '{print $4}' | sort -u | wc -l | tr -d ' ')
    [ "$COUNT" = "1" ] || { echo "Final checksums differ"; FAILED=1; }
fi
rm -rf "$DIR" mp-template-"$PORT".map mp-session-"$PORT"-p*.sav mp-session-"$PORT"-p*.mpsav mp-desync-"$PORT"-*.sav mp-desync-"$PORT"-*.mpsav \
    lan-save-"$PORT"-p*.mpsav mp-autosave-"$PORT"-p*.tmp lan-autosave-"$PORT"-*.mpsav lan-autosave-"$PORT"-*.mpsav.drag
exit $FAILED
