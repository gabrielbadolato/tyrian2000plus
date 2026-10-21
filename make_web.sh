#!/bin/bash
# make_web.sh — build Tyrian 2000+ for web browsers (the "play in browser"
# version on the itch.io page) into dist/Tyrian2000Plus-web/.
#
# Copyright (C) 2026 Tyrian 2000+ contributors
#
# The same C code is compiled to WebAssembly with Emscripten, against
# Emscripten's own SDL2 (built here from the same SDL2 source as
# make_linux.sh).  ASYNCIFY lets every screen of the game keep its own loop:
# each SDL_Delay hands control back to the browser, so nothing in the game
# had to be rewritten.  The page itself is web/index.html; it loads the game
# data from data/ and keeps settings and saves in the browser (IndexedDB).
#
# Like the other packages, a normal build has NO game data.  --with-data adds
# the original data, unchanged (add_game_data.sh), which is what releases do.
#
# Everything downloaded is checked by a fixed sha256.  Only python3, node,
# curl and xz are needed from the system.
#
# Usage: ./make_web.sh [--with-data]
#   Test locally: python3 -m http.server -d dist/Tyrian2000Plus-web 8000
#   and open http://localhost:8000 (the page does not work from file://).
set -euo pipefail

cd "$(dirname "$0")"

WITH_DATA=false
[ "${1:-}" = "--with-data" ] && WITH_DATA=true

# Emscripten 6.0.13 (the "wasm-binaries" archive emsdk would install).
# To update: take the release hash and sha256 of the new version together.
EMSDK_VER="6.0.13"
EMSDK_HASH="eb132757325583b787fe9414b09afea9442b72ad"
EMSDK_SHA256="cda6801012ff3319a539111826ed02a9abd7eff135c2ef261c8941caf5a3763e"
SDL2_VER="2.32.10"
SDL2_SHA256="5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165"
TOOLS="$PWD/build/web"
NAME="Tyrian2000Plus-web"
OUT="dist/$NAME"

check_sha256() {  # $1 = file, $2 = expected sha256
    local got
    got=$(sha256sum "$1" | cut -d' ' -f1)
    if [ "$got" != "$2" ]; then
        echo "ERROR: $1 has sha256 $got, expected $2" >&2
        rm -f "$1"
        exit 1
    fi
}

mkdir -p "$TOOLS"

# ---- Emscripten ----
EMSDK="$TOOLS/emscripten-$EMSDK_VER"
if [ ! -x "$EMSDK/install/emscripten/emcc" ]; then
    echo "Downloading Emscripten $EMSDK_VER ..."
    [ -f "$TOOLS/wasm-binaries.tar.xz" ] || curl -fsSL --retry 4 -o "$TOOLS/wasm-binaries.tar.xz" \
        "https://storage.googleapis.com/webassembly/emscripten-releases-builds/linux/$EMSDK_HASH/wasm-binaries.tar.xz"
    check_sha256 "$TOOLS/wasm-binaries.tar.xz" "$EMSDK_SHA256"
    rm -rf "$EMSDK"
    mkdir -p "$EMSDK"
    tar -xJf "$TOOLS/wasm-binaries.tar.xz" -C "$EMSDK"
    rm "$TOOLS/wasm-binaries.tar.xz"
fi
# The configuration emsdk would write, with the system's node.
export EM_CONFIG="$TOOLS/emscripten.config"
cat > "$EM_CONFIG" <<EOF
LLVM_ROOT = '$EMSDK/install/bin'
BINARYEN_ROOT = '$EMSDK/install'
NODE_JS = '$(command -v node)'
EOF
export EM_CACHE="$TOOLS/cache"
EMCC="$EMSDK/install/emscripten/emcc"

# ---- SDL2 source, for Emscripten's SDL2 port ----
SDL_SRC="$TOOLS/SDL2-$SDL2_VER"
if [ ! -f "$SDL_SRC/include/SDL.h" ]; then
    echo "Downloading SDL2 $SDL2_VER ..."
    curl -fsSL --retry 4 -o "$TOOLS/SDL2.tar.gz" \
        "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-$SDL2_VER.tar.gz"
    check_sha256 "$TOOLS/SDL2.tar.gz" "$SDL2_SHA256"
    rm -rf "$SDL_SRC"
    tar -xzf "$TOOLS/SDL2.tar.gz" -C "$TOOLS"
    rm "$TOOLS/SDL2.tar.gz"
fi
export EMCC_LOCAL_PORTS="sdl2=$SDL_SRC"

# ---- the game ----
SDL_FLAGS="-sUSE_SDL=2"
LINK_FLAGS="-sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 \
    -sALLOW_MEMORY_GROWTH -sSTACK_SIZE=1048576 \
    -sEXIT_RUNTIME -sFORCE_FILESYSTEM -lidbfs.js \
    -sEXPORTED_RUNTIME_METHODS=callMain,FS,IDBFS,addRunDependency,removeRunDependency \
    -sINVOKE_RUN=0 -sENVIRONMENT=web"
rm -rf "$OUT"
mkdir -p "$OUT/data" "$OUT/licencas"
make clean >/dev/null
make -j"$(nproc 2>/dev/null || echo 4)" \
    CC="$EMCC" WITH_NETWORK=false PKG_CONFIG=false \
    SDL_CPPFLAGS="$SDL_FLAGS" SDL_LDFLAGS="$SDL_FLAGS $LINK_FLAGS" SDL_LDLIBS= \
    TARGET="$OUT/tyrian.js" \
    "$OUT/tyrian.js"
make clean >/dev/null  # so a native build after this starts clean

# ---- the page and the notes ----
cp web/index.html "$OUT/index.html"
cp linux/icons/tyrian2000-128.png "$OUT/icon.png"
cp COPYING "$OUT/licencas/GPL-2.0.txt"
cp doc/tyrian-freeware-license.txt "$OUT/licencas/tyrian-freeware-license.txt"
cp "$SDL_SRC/LICENSE.txt" "$OUT/licencas/SDL2.txt"
./write_source_note.sh "$OUT"

if $WITH_DATA; then
    ./add_game_data.sh "$OUT"
fi
# The page loads the data as one plain tar (the files inside unchanged): one
# download instead of ~90, and names like "newsh#.shp" need no escaping.
if [ -f "$OUT/data/tyrian1.lvl" ]; then
    (cd "$OUT/data" &&
        tar --format=ustar --sort=name --owner=0 --group=0 --numeric-owner \
            --exclude='*.txt' -cf ../tyrian2000.tar . &&
        find . -type f ! -name '*.txt' -delete &&
        mv ../tyrian2000.tar .)
fi

echo
if [ -f "$OUT/data/tyrian2000.tar" ]; then
    echo "built: $OUT (with the game data)"
else
    echo "built: $OUT (no game data; see ./make_web.sh --with-data)"
fi
