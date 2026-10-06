#!/usr/bin/env bash
# Runs a command alone among the worktrees of this repository: the network tests use fixed ports and the
# automation writes in the shared game data, so two worktrees must not run them at the same time.
#
# Usage: tools/serial.sh COMMAND [ARGS...]
set -uo pipefail
LOCK="$(git -C "$(dirname "$0")/.." rev-parse --path-format=absolute --git-common-dir)/serial.lock"
while ! mkdir "$LOCK" 2>/dev/null; do
    holder=$(cat "$LOCK/pid" 2>/dev/null || true)
    if [ -n "$holder" ] && ! kill -0 "$holder" 2>/dev/null; then
        rm -rf "$LOCK" # its owner is gone
        continue
    fi
    sleep 5
done
echo $$ > "$LOCK/pid"
trap 'rm -rf "$LOCK"' EXIT
"$@"
