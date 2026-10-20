#!/bin/bash
# tests/run_autoplay.sh — play every episode on every difficulty, without a
# screen, and report crashes, memory errors and levels that never end.  Then
# press random keys through the menus (tests/test_monkey.c).
#
#   tests/run_autoplay.sh <data-dir> [episodes] [difficulties] [routes] [seeds]
#
# <data-dir> holds the original Tyrian 2000 data (tyrian1.lvl etc.).  By
# default it plays episodes 1-5 on the difficulties of the menu (Easy, Normal,
# Hard, Impossible, Suicide, Lord of the Game), each by two routes (where the
# map offers a choice, route 0 takes the last planet and route 1 the first),
# several at once; then four 30-minute runs of random keys (seeds 1-4).  Built
# with AddressSanitizer and UndefinedBehaviorSanitizer; see the two tests.
#
# Copyright (C) 2026 Tyrian 2000+ contributors
set -euo pipefail

DATA="$(cd "$1" && pwd)"
EPISODES="${2:-1 2 3 4 5}"
DIFFICULTIES="${3:-1 2 3 4 6 8}"
ROUTES="${4:-0 1}"
SEEDS="${5:-1 2 3 4}"

cd "$(dirname "$0")/.."
OUT=build/autoplay
mkdir -p "$OUT/obj" "$OUT/logs"

if [ -f build/sdl/lib/pkgconfig/sdl2.pc ]; then
    export PKG_CONFIG_PATH="$PWD/build/sdl/lib/pkgconfig"
fi
CFLAGS="-std=iso9899:1999 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -DTARGET_UNIX $(pkg-config --cflags sdl2)"
LIBS="$(pkg-config --static --libs sdl2) -lm"

for f in src/*.c; do
    o="$OUT/obj/$(basename "${f%.c}").o"
    extra=""
    [ "$f" = src/opentyr.c ] && extra="-Dmain=opentyrian_main"
    cc $CFLAGS $extra -Isrc -c -o "$o" "$f"
done
# The test replaces SDL's clock and wraps the item screen (see test_autoplay.c).
cc $CFLAGS -Isrc -o "$OUT/test_autoplay" tests/test_autoplay.c "$OUT"/obj/*.o $LIBS \
    -Wl,--wrap=SDL_GetTicks,--wrap=SDL_Delay,--wrap=JE_itemScreen
cc $CFLAGS -Isrc -o "$OUT/test_monkey" tests/test_monkey.c "$OUT"/obj/*.o $LIBS \
    -Wl,--wrap=SDL_GetTicks,--wrap=SDL_Delay,--wrap=SDL_PollEvent,--wrap=SDL_OpenURL \
    -Wl,--wrap=update_check_start,--wrap=titleScreen,--wrap=JE_main

# run <log> <what> <program> <args...>: each run with its own empty user folder
run() {
    local log=$1 what=$2 home
    shift 2
    home=$(mktemp -d)
    # halt_on_error: a memory error stops the run and is reported below
    if HOME="$home" XDG_CONFIG_HOME="$home" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
       ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
       timeout 1800 "$@" > "$log" 2>&1; then
        echo "$what: $(grep '^ok:' "$log")"
    else
        echo "FAIL: $what (log in $log):"
        grep -E '^FAIL|ERROR|runtime error|SUMMARY' "$log" | head -5 || tail -5 "$log"
    fi
    rm -rf "$home"
}

run_one() {
    run "$OUT/logs/e$1-d$2-r$3.txt" "episode $1, difficulty $2, route $3" \
        "$OUT/test_autoplay" "$DATA" "$@"
}

run_monkey() {
    run "$OUT/logs/keys-$1.txt" "random keys, seed $1" "$OUT/test_monkey" "$DATA" "$1" 30
}
export -f run run_one run_monkey
export OUT DATA

for ep in $EPISODES; do
    for diff in $DIFFICULTIES; do
        for route in $ROUTES; do
            echo "$ep $diff $route"
        done
    done
done | xargs -P "$(nproc)" -n 3 bash -c 'run_one "$@"' _ | tee "$OUT/summary.txt"

for seed in $SEEDS; do
    echo "$seed"
done | xargs -P "$(nproc)" -n 1 bash -c 'run_monkey "$@"' _ | tee -a "$OUT/summary.txt"

! grep -q '^FAIL' "$OUT/summary.txt"
