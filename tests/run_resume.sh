#!/bin/bash
# tests/run_resume.sh — check the level recording (src/levelrec.c): Continue
# after the game closed in the middle of a level, and Replay.
#
#   tests/run_resume.sh <data-dir> [episodes] [frames] [difficulty] [players]
#
# <data-dir> holds the original Tyrian 2000 data.  For each episode (default
# 1-5) it plays every level for [frames] frames (default 2100, a minute) and
# copies the user folder there, as if the game had closed; then, from each
# copy, resumes the level (it must come out the same, frame by frame, and go
# on), resumes it again from a little later, and replays it.  Built with AddressSanitizer and UndefinedBehaviorSanitizer;
# see tests/test_resume.c.  TEST_MANUAL=1 plays the levels with the fire
# and sidekick keys and Auto Fire instead of constant play.
#
# Copyright (C) 2026 Tyrian 2000+ contributors
set -euo pipefail

DATA="$(cd "$1" && pwd)"
EPISODES="${2:-1 2 3 4 5}"
FRAMES="${3:-2100}"
DIFFICULTY="${4:-2}"
PLAYERS="${5:-1}"

cd "$(dirname "$0")/.."
OUT=${OUT:-build/resume}
RUNS="$OUT/runs-$PLAYERS-$DIFFICULTY"
if [ "${TEST_MANUAL:-0}" = 1 ]; then RUNS="$RUNS-manual"; fi
rm -rf "$RUNS"
mkdir -p "$OUT/obj" "$RUNS"

if [ -f build/sdl/lib/pkgconfig/sdl2.pc ]; then
    export PKG_CONFIG_PATH="$PWD/build/sdl/lib/pkgconfig"
fi
CFLAGS="-std=iso9899:1999 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -DTARGET_UNIX $(pkg-config --cflags sdl2)"
LIBS="$(pkg-config --static --libs sdl2) -lm"

# SKIP_BUILD=1: use the program built by the last run
if [ "${SKIP_BUILD:-0}" != 1 ]; then
for f in src/*.c; do
    o="$OUT/obj/$(basename "${f%.c}").o"
    extra=""
    [ "$f" = src/opentyr.c ] && extra="-Dmain=opentyrian_main"
    cc $CFLAGS $extra -Isrc -c -o "$o" "$f"
done
cc $CFLAGS -Isrc -o "$OUT/test_resume" tests/test_resume.c "$OUT"/obj/*.o $LIBS \
    -Wl,--wrap=SDL_GetTicks,--wrap=SDL_Delay,--wrap=JE_pauseGame,--wrap=levelrec_frame,--wrap=levelrec_level_begin,--wrap=levelrec_level_end
fi

# run <home> <log> <args...>
run() {
    local home=$1 log=$2
    shift 2
    HOME="$home" XDG_CONFIG_HOME="$home" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
    timeout 900 "$OUT/test_resume" "$DATA" "$@" > "$log" 2>&1
}

failed=0
for e in $EPISODES; do
    dir="$RUNS/ep$e"
    mkdir -p "$dir/home/tyrian2000plus" "$dir/copies"
    if ! run "$dir/home" "$dir/record.log" record "$e" "$DIFFICULTY" "$FRAMES" "$PWD/$dir/copies" "$e" "$PLAYERS"; then
        echo "FAIL: recording episode $e (log in $dir/record.log)"
        tail -5 "$dir/record.log"
        failed=1
    fi
    for copy in "$dir"/copies/*/; do
        [ -d "$copy" ] || continue
        name="ep$e/$(basename "$copy") $(cut -d' ' -f2 "$copy/level.txt")"
        # Continue (and once more from where that one stopped), then Replay
        for what in resume resume-again watch; do
            home=$(mktemp -d)
            mkdir -p "$home/tyrian2000plus"
            from="$copy"
            [ "$what" != resume ] && from="$copy/again"
            cp "$from"/lastlevel.rec "$from"/tyrian.sav "$from"/opentyrian.cfg "$home/tyrian2000plus/"
            log="$copy/$what.log"
            case $what in
                resume) args=(resume 350 "$PWD/$copy/again") ;;
                resume-again) args=(resume 350) ;;
                watch) args=(watch) ;;
            esac
            if run "$home" "$log" "${args[@]}"; then
                echo "$name $what: $(grep -E '^ok:' "$log")"
            else
                echo "FAIL: $name $what (log in $log):"
                grep -E '^FAIL|ERROR|WARN|runtime error|SUMMARY|Level recording' "$log" | head -5 || tail -5 "$log"
                failed=1
                rm -rf "$home"
                break
            fi
            rm -rf "$home"
        done
    done
done

exit $failed
