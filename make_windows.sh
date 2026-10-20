#!/bin/bash
# make_windows.sh — cross-compile Tyrian2000Plus.exe for Windows x64 on Linux
# and pack it into dist/Tyrian2000Plus-windows-x64.zip.
#
# Uses llvm-mingw (clang) and the official SDL2/SDL2_net mingw development
# releases; all three are downloaded into build/ if missing.  The game is
# compiled against SDL2, but the package ships sdl2-compat (an SDL2.dll that
# runs on top of SDL3.dll): only SDL3 talks to the 2026 Steam Controller
# directly, without Steam.  If
# x86_64-w64-mingw32-clang is already on the PATH, that one is used instead.
#
# The Tyrian 2000 data is NOT included in the package; the user copies it
# into the data/ folder next to the exe (see README.md).
#
# Build requirements: make, curl, tar, xz, zip, unzip.
# Usage: ./make_windows.sh
set -euo pipefail

cd "$(dirname "$0")"

LLVM_MINGW_VER="20260922"
SDL2_VER="2.32.10"
SDL2_NET_VER="2.4.0"
SDL2_COMPAT_VER="2.32.74"  # bundles SDL3 3.4.18
# sha256 of each download above: a changed file stops the build instead of
# ending up in the package.  Update them together with the versions.
LLVM_MINGW_SHA256="bb7bb7654b33d5aa8712acb837c963b2e0c56352560c76105270a3268c665c21"
SDL2_SHA256="83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2"
SDL2_NET_SHA256="0d6de8d1f9c369beea85d9d7a45a0b3e64eb9288c166b9ea84a961cc34d0ba6f"
SDL2_COMPAT_SHA256="94a0184bc2d0670151a023656fb58e8f722fc001bda3a6a424234c88c5501cc0"
NPROC="$(nproc 2>/dev/null || echo 4)"
NAME="Tyrian2000Plus-windows-x64"
VENDOR="$PWD/build/win"

mkdir -p "$VENDOR"

check_sha256() {  # $1 = file, $2 = expected sha256
    local got
    got=$(sha256sum "$1" | cut -d' ' -f1)
    if [ "$got" != "$2" ]; then
        echo "ERROR: $1 has sha256 $got, expected $2" >&2
        rm -f "$1"
        exit 1
    fi
}

if ! command -v x86_64-w64-mingw32-clang >/dev/null; then
    LLVM_MINGW="llvm-mingw-$LLVM_MINGW_VER-ucrt-ubuntu-22.04-x86_64"
    if [ ! -x "$VENDOR/$LLVM_MINGW/bin/x86_64-w64-mingw32-clang" ]; then
        echo "Downloading llvm-mingw $LLVM_MINGW_VER ..."
        curl -fsSL -o "$VENDOR/llvm-mingw.tar.xz" \
            "https://github.com/mstorsjo/llvm-mingw/releases/download/$LLVM_MINGW_VER/$LLVM_MINGW.tar.xz"
        check_sha256 "$VENDOR/llvm-mingw.tar.xz" "$LLVM_MINGW_SHA256"
        tar -xJf "$VENDOR/llvm-mingw.tar.xz" -C "$VENDOR"
        rm "$VENDOR/llvm-mingw.tar.xz"
    fi
    export PATH="$VENDOR/$LLVM_MINGW/bin:$PATH"
fi

fetch_devel() {  # $1 = repo, $2 = package name, $3 = version, $4 = sha256
    if [ ! -d "$VENDOR/$2-$3" ]; then
        echo "Downloading $2 $3 (mingw) ..."
        curl -fsSL -o "$VENDOR/$2.tar.gz" \
            "https://github.com/libsdl-org/$1/releases/download/release-$3/$2-devel-$3-mingw.tar.gz"
        check_sha256 "$VENDOR/$2.tar.gz" "$4"
        tar -xzf "$VENDOR/$2.tar.gz" -C "$VENDOR"
        rm "$VENDOR/$2.tar.gz"
    fi
}
fetch_devel SDL SDL2 "$SDL2_VER" "$SDL2_SHA256"
fetch_devel SDL_net SDL2_net "$SDL2_NET_VER" "$SDL2_NET_SHA256"
if [ ! -d "$VENDOR/sdl2-compat-$SDL2_COMPAT_VER" ]; then
    echo "Downloading sdl2-compat $SDL2_COMPAT_VER ..."
    curl -fsSL -o "$VENDOR/sdl2-compat.zip" \
        "https://github.com/libsdl-org/sdl2-compat/releases/download/release-$SDL2_COMPAT_VER/sdl2-compat-$SDL2_COMPAT_VER-win32-x64.zip"
    check_sha256 "$VENDOR/sdl2-compat.zip" "$SDL2_COMPAT_SHA256"
    mkdir -p "$VENDOR/sdl2-compat-$SDL2_COMPAT_VER"
    (cd "$VENDOR/sdl2-compat-$SDL2_COMPAT_VER" && unzip -q ../sdl2-compat.zip)
    rm "$VENDOR/sdl2-compat.zip"
fi
COMPAT="$VENDOR/sdl2-compat-$SDL2_COMPAT_VER"
SDL="$VENDOR/SDL2-$SDL2_VER/x86_64-w64-mingw32"
NET="$VENDOR/SDL2_net-$SDL2_NET_VER/x86_64-w64-mingw32"

# ---- the game ----
make clean >/dev/null
make -j"$NPROC" PLATFORM=WIN32 WITH_NETWORK=true \
    CC=x86_64-w64-mingw32-clang WINDRES=x86_64-w64-mingw32-windres \
    SDL_CPPFLAGS="-I$SDL/include -I$SDL/include/SDL2 -I$NET/include -I$NET/include/SDL2" \
    SDL_LDFLAGS="-L$SDL/lib -L$NET/lib -mwindows" \
    SDL_LDLIBS="-lmingw32 -lSDL2main -lSDL2_net -lSDL2"

# ---- package ----
rm -rf "dist/$NAME" "dist/$NAME.zip"
mkdir -p "dist/$NAME/data" "dist/$NAME/licencas"
cp opentyrian2000.exe "dist/$NAME/Tyrian2000Plus.exe"
x86_64-w64-mingw32-strip "dist/$NAME/Tyrian2000Plus.exe"
cp "$COMPAT/SDL2.dll" "$COMPAT/SDL3.dll" "$NET/bin/SDL2_net.dll" "dist/$NAME/"
sed -e '/^<table>/,/^<\/table>/d' -e 's/$/\r/' README.pt-BR.md > "dist/$NAME/LEIA-ME.txt"
sed -e '/^<table>/,/^<\/table>/d' -e 's/$/\r/' README.md > "dist/$NAME/README.txt"
sed 's/$/\r/' COPYING > "dist/$NAME/licencas/GPL-2.0.txt"
./write_source_note.sh "dist/$NAME" crlf
sed 's/$/\r/' doc/tyrian-freeware-license.txt > "dist/$NAME/licencas/tyrian-freeware-license.txt"
cp "$COMPAT/LICENSE.txt" "dist/$NAME/licencas/SDL2-SDL3.txt"
cp "$VENDOR/SDL2_net-$SDL2_NET_VER/LICENSE.txt" "dist/$NAME/licencas/SDL2_net.txt"
printf '%s\r\n' 'Copie para esta pasta todos os arquivos da pasta "tyrian2000" de' \
    'https://www.camanis.net/tyrian/tyrian2000.zip (tyrian.hdt, tyrian1.lvl,' \
    'music.mus etc.). Veja LEIA-ME.txt.' > "dist/$NAME/data/COLOQUE-OS-DADOS-AQUI.txt"
(cd dist && zip -qr "$NAME.zip" "$NAME")

echo
echo "built: dist/$NAME.zip"
