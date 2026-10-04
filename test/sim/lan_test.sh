#!/bin/sh
# Network game test: one host and N-1 clients, headless, on this computer.
# Usage: lan_test.sh SIMTOOL PORT PLAYERS SAVE TICKS [desync]
# Without 'desync': every player must end with the same checksum and the host must verify every turn.
# With 'desync': the last client changes its own state; host and client must detect it.
SIMTOOL=$1; PORT=$2; PLAYERS=$3; SAVE=$4; TICKS=$5; MODE=$6
DIR=$(mktemp -d)
HOST_ARGS=""
[ "$MODE" = "desync" ] && HOST_ARGS="expect-desync"
"$SIMTOOL" mpnode host "$PORT" "$PLAYERS" "$SAVE" "$TICKS" $HOST_ARGS > "$DIR/host.log" 2>&1 &
HOST=$!
sleep 0.3
PIDS=""
i=1
while [ $i -lt "$PLAYERS" ]; do
    CLIENT_MODE=""
    [ "$MODE" = "desync" ] && CLIENT_MODE="expect-desync"
    [ "$MODE" = "desync" ] && [ $i -eq $((PLAYERS - 1)) ] && CLIENT_MODE="desync"
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
if [ "$MODE" != "desync" ]; then
    COUNT=$(grep -h "checksum" "$DIR"/*.log | awk '{print $4}' | sort -u | wc -l | tr -d ' ')
    [ "$COUNT" = "1" ] || { echo "Final checksums differ"; FAILED=1; }
fi
rm -rf "$DIR" mp-session-"$PORT"-p*.sav mp-desync-"$PORT"-*.sav
exit $FAILED
