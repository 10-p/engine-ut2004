#!/usr/bin/env bash
# run.sh -- run the native build under Xvfb with the input test channel and feed it a scenario.
#
#   tools/dev/inputtest/run.sh <run-tree> <scenario> <out-dir> [engine args...]
#   tools/dev/inputtest/run.sh run move.scen out/move 'DM-Rankin?Game=XGame.xDeathMatch' -SOFTWARERENDERER
#
# A scenario is a list of the channel's commands (Source/SDL2Drv/Src/SDL2InputTest.h), one per line, with two
# more that run here: "sleep <seconds>" and "waitlog <extended regex>" (wait until the log matches it, 120 s at
# most). Lines starting with # are comments. The game's log goes to <out-dir>/game.log, the commands it was
# given to <out-dir>/cmds.txt; rate.py beside this script reads the watch lines of the log. The game is the run
# tree's System/ut2004-pixo.bin, else System/ut2004.bin.
#
# Xvfb takes the first free display (-displayfd); the game and Xvfb are stopped when the scenario ends.
set -u
RUN="$(cd "${1:?usage: run.sh <run-tree> <scenario> <out-dir> [engine args...]}" && pwd)"
SCEN="$(cd "$(dirname "${2:?}")" && pwd)/$(basename "$2")"
OUT="${3:?}"
shift 3
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
CMD="$OUT/cmds.txt"
LOG="$OUT/game.log"
: > "$CMD"
: > "$LOG"
BIN=ut2004-pixo.bin
[ -x "$RUN/System/$BIN" ] || BIN=ut2004.bin

Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$OUT/display" 2>"$OUT/xvfb.log" &
XPID=$!
for _ in $(seq 50); do [ -s "$OUT/display" ] && break; sleep 0.1; done
DISP=":$(cat "$OUT/display")"

cd "$RUN/System"
DISPLAY="$DISP" LD_LIBRARY_PATH="$RUN/System:${LD_LIBRARY_PATH:-}" stdbuf -oL \
    "./$BIN" "$@" -log -INPUTTEST="$CMD" > "$LOG" 2>&1 &
GPID=$!

while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in
        "#"*|"") ;;
        sleep\ *) sleep "${line#sleep }" ;;
        waitlog\ *)
            re="${line#waitlog }"
            for _ in $(seq 600); do
                grep -qE "$re" "$LOG" && break
                kill -0 $GPID 2>/dev/null || { echo "run.sh: the game ended before: $re" >&2; break; }
                sleep 0.2
            done ;;
        *) printf '%s\n' "$line" >> "$CMD" ;;
    esac
done < "$SCEN"

sleep 1
kill -TERM $GPID 2>/dev/null
for _ in $(seq 50); do kill -0 $GPID 2>/dev/null || break; sleep 0.1; done
kill -KILL $GPID 2>/dev/null
kill -TERM $XPID 2>/dev/null
wait 2>/dev/null
echo "$LOG"
