#!/bin/bash
# tests/run_tests.sh — build and run the headless tests against the system SDL2.
#
#   tests/run_tests.sh [game-dir]
#
# test_gamepad always runs.  test_saves needs the real game: a directory with
# data/, tyrian.sav and tyrian.cfg (none of them are in the repository), so it
# only runs when given one.
set -euo pipefail

cd "$(dirname "$0")/.."
OUT=build/tests
mkdir -p "$OUT/obj"

# The tests need SDL 2.24 or newer (virtual joysticks).  Prefer the static SDL
# built by make_linux.sh, since distro SDL2 can be older than that.
if [ -f build/sdl/lib/pkgconfig/sdl2.pc ]; then
    export PKG_CONFIG_PATH="$PWD/build/sdl/lib/pkgconfig"
fi
CFLAGS="-std=iso9899:1999 -O1 -g -DTARGET_UNIX $(pkg-config --cflags sdl2)"
LIBS="$(pkg-config --static --libs sdl2) -lm"

for f in src/*.c; do
    o="$OUT/obj/$(basename "${f%.c}").o"
    extra=""
    [ "$f" = src/opentyr.c ] && extra="-Dmain=opentyrian_main"
    cc $CFLAGS $extra -Isrc -c -o "$o" "$f"
done

cc $CFLAGS -Isrc -o "$OUT/test_gamepad" tests/test_gamepad.c "$OUT"/obj/*.o $LIBS
cc $CFLAGS -Isrc -o "$OUT/test_saves" tests/test_saves.c "$OUT"/obj/*.o $LIBS

echo "== test_gamepad"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$OUT/test_gamepad"

if [ "$#" -ge 1 ]; then
    echo "== test_saves"
    tmp=$(mktemp -d)
    mkdir -p "$tmp/tyrian2000plus"
    cp "$1/tyrian.sav" "$1/tyrian.cfg" "$tmp/tyrian2000plus/"
    # the game looks for data/ next to the executable
    ln -sfn "$(cd "$1" && pwd)/data" "$OUT/data"
    XDG_CONFIG_HOME="$tmp" "$OUT/test_saves" "$tmp/tyrian2000plus"
    rm -f "$OUT/data"
    rm -rf "$tmp"
fi
