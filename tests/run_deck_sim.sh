#!/bin/bash
# tests/run_deck_sim.sh — run the game as on a Steam Deck in Game Mode and
# take screenshots, following a script of button presses (see tests/deck_sim.c).
#
#   tests/run_deck_sim.sh <data-dir> <script> <shot-dir> [user-dir]
#
# The screen is a virtual 1280x800 one (Xvfb) with a window manager that makes
# the window fill it (matchbox-window-manager), as gamescope does on the Deck;
# screenshots need ImageMagick's "import".  Without [user-dir] the game starts
# with no settings, as on its first start.  Uses build/sdl from make_linux.sh.
#
# Copyright (C) 2026 Tyrian 2000+ contributors
set -euo pipefail

DATA="$(cd "$1" && pwd)"
SCRIPT="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"
mkdir -p "$3"
SHOTS="$(cd "$3" && pwd)"
USERDIR="${4:-$(mktemp -d)}"
mkdir -p "$USERDIR"

cd "$(dirname "$0")/.."
OUT=build/deck_sim
mkdir -p "$OUT/obj"

[ -f build/sdl/lib/pkgconfig/sdl2.pc ] || { echo "run ./make_linux.sh first" >&2; exit 1; }
export PKG_CONFIG_PATH="$PWD/build/sdl/lib/pkgconfig"
CFLAGS="-std=iso9899:1999 -O1 -g -DTARGET_UNIX $(pkg-config --cflags sdl2)"
LIBS="$(pkg-config --static --libs sdl2) -lm"

for f in src/*.c; do
    o="$OUT/obj/$(basename "${f%.c}").o"
    extra=""
    [ "$f" = src/opentyr.c ] && extra="-Dmain=opentyrian_main"
    cc $CFLAGS $extra -Isrc -c -o "$o" "$f" &
done
wait
cc $CFLAGS -Isrc -o "$OUT/deck_sim" tests/deck_sim.c "$OUT"/obj/*.o $LIBS \
    -Wl,--wrap=SDL_PollEvent -Wl,--wrap=SDL_OpenURL

DISPLAY_NUM=:77
Xvfb $DISPLAY_NUM -screen 0 1280x800x24 -nolisten tcp >/dev/null 2>&1 &
XVFB=$!
sleep 1
DISPLAY=$DISPLAY_NUM matchbox-window-manager -use_titlebar no >/dev/null 2>&1 &
WM=$!
trap 'kill $WM $XVFB 2>/dev/null' EXIT
sleep 1

DISPLAY=$DISPLAY_NUM HOME="$USERDIR" XDG_CONFIG_HOME="$USERDIR/.config" SDL_AUDIODRIVER=dummy \
    timeout 600 "$OUT/deck_sim" "$DATA" "$SCRIPT" "$SHOTS"
