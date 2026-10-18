#!/bin/bash
# make_linux.sh — build a self-contained Tyrian2000Plus for Linux (and the
# Steam Deck) and pack it into dist/Tyrian2000Plus-linux-x86_64.tar.gz.
#
# SDL2 and SDL2_net are built from source and linked STATICALLY.  A distro's
# libSDL2 is linked against whatever that distro has (libdecor, Wayland,
# PulseAudio, PipeWire...), and bundling it drags every one of those in as a
# hard dependency on the user's machine.  SDL built from source instead
# loads all of those backends with dlopen at run time, so the resulting
# binary needs nothing beyond glibc and libm - and whatever the user has
# (X11 or Wayland, ALSA, Pulse or PipeWire) gets picked up when it runs.
# Build on the oldest distro you want to support (CI uses Ubuntu 22.04):
# the binary needs a glibc no newer than the one it was built against.
#
# The Tyrian 2000 data is NOT included in the package; the user copies it
# into the data/ folder next to the binary (see doc/LEIA-ME.txt).
#
# Build requirements (headers only; nothing is linked from them):
#   Debian/Ubuntu:  sudo apt install build-essential cmake pkg-config curl \
#       libasound2-dev libpulse-dev libpipewire-0.3-dev libx11-dev libxext-dev \
#       libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
#       libwayland-dev wayland-protocols libxkbcommon-dev libdecor-0-dev \
#       libegl1-mesa-dev libgl1-mesa-dev libgles2-mesa-dev libgbm-dev \
#       libdrm-dev libdbus-1-dev libudev-dev
#
# Usage: ./make_linux.sh
set -euo pipefail

cd "$(dirname "$0")"

SDL2_VER="2.32.10"
SDL2_NET_VER="2.4.0"
PREFIX="$PWD/build/sdl"
NPROC="$(nproc 2>/dev/null || echo 4)"
NAME="Tyrian2000Plus-linux-x86_64"

# The license of anything we link in has to travel with the binary.  Keep the
# copies inside the prefix, since that is what CI caches: on a cache hit the
# source trees below are never unpacked.
install_license() {  # $1 = source directory, $2 = name in the package
    mkdir -p "$PREFIX/share/licenses"
    for name in LICENSE.txt LICENSE COPYING.txt COPYING; do
        if [ -f "$1/$name" ]; then
            cp "$1/$name" "$PREFIX/share/licenses/$2.txt"
            return 0
        fi
    done
    echo "ERROR: no license file found in $1" >&2
    exit 1
}

# ---- static SDL2 + SDL2_net from source, installed under build/sdl ----
if [ ! -f "$PREFIX/lib/libSDL2.a" ]; then
    echo "Building SDL2 $SDL2_VER (static) ..."
    mkdir -p build/vendor
    curl -fsSL -o build/vendor/SDL2.tar.gz \
        "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-$SDL2_VER.tar.gz"
    rm -rf "build/vendor/SDL2-$SDL2_VER"
    tar -xzf build/vendor/SDL2.tar.gz -C build/vendor
    cmake -S "build/vendor/SDL2-$SDL2_VER" -B "build/vendor/SDL2-$SDL2_VER/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DSDL_SHARED=OFF \
        -DSDL_STATIC=ON \
        -DSDL_STATIC_PIC=ON \
        -DSDL_TEST=OFF >/dev/null
    cmake --build "build/vendor/SDL2-$SDL2_VER/build" -j"$NPROC" >/dev/null
    cmake --install "build/vendor/SDL2-$SDL2_VER/build" >/dev/null
    install_license "build/vendor/SDL2-$SDL2_VER" SDL2
fi

if [ ! -f "$PREFIX/lib/libSDL2_net.a" ]; then
    echo "Building SDL2_net $SDL2_NET_VER (static) ..."
    mkdir -p build/vendor
    curl -fsSL -o build/vendor/SDL2_net.tar.gz \
        "https://github.com/libsdl-org/SDL_net/releases/download/release-$SDL2_NET_VER/SDL2_net-$SDL2_NET_VER.tar.gz"
    rm -rf "build/vendor/SDL2_net-$SDL2_NET_VER"
    tar -xzf build/vendor/SDL2_net.tar.gz -C build/vendor
    cmake -S "build/vendor/SDL2_net-$SDL2_NET_VER" -B "build/vendor/SDL2_net-$SDL2_NET_VER/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_PREFIX_PATH="$PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDL2NET_SAMPLES=OFF >/dev/null
    cmake --build "build/vendor/SDL2_net-$SDL2_NET_VER/build" -j"$NPROC" >/dev/null
    cmake --install "build/vendor/SDL2_net-$SDL2_NET_VER/build" >/dev/null
    install_license "build/vendor/SDL2_net-$SDL2_NET_VER" SDL2_net
fi

# ---- the game, against the static SDL ----
# A static SDL2_net installs no .pc file, so its flags are given by hand;
# --static pulls in the libraries SDL itself needs (m, dl, pthread, rt).
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"
make clean >/dev/null
make -j"$NPROC" WITH_NETWORK=true \
    SDL_CPPFLAGS="$(pkg-config --cflags sdl2)" \
    SDL_LDFLAGS="$(pkg-config --static --libs-only-L sdl2)" \
    SDL_LDLIBS="-lSDL2_net $(pkg-config --static --libs-only-l sdl2)"

if ldd opentyrian2000 | grep -q "libSDL2"; then
    echo "ERROR: opentyrian2000 still links SDL2 dynamically" >&2
    exit 1
fi

# ---- package ----
rm -rf "dist/$NAME" "dist/$NAME.tar.gz"
mkdir -p "dist/$NAME/data" "dist/$NAME/licencas"
cp opentyrian2000 "dist/$NAME/Tyrian2000Plus"
strip "dist/$NAME/Tyrian2000Plus"
cp doc/LEIA-ME.txt "dist/$NAME/"
cp linux/icons/tyrian2000-128.png "dist/$NAME/Tyrian2000Plus.png"
cp COPYING "dist/$NAME/licencas/GPL-2.0.txt"
cp doc/tyrian-freeware-license.txt "dist/$NAME/licencas/tyrian-freeware-license.txt"
cp "$PREFIX/share/licenses/SDL2.txt" "$PREFIX/share/licenses/SDL2_net.txt" "dist/$NAME/licencas/"
cat > "dist/$NAME/data/COLOQUE-OS-DADOS-AQUI.txt" <<'EOF'
Copie para esta pasta todos os arquivos da pasta "tyrian2000" de
https://www.camanis.net/tyrian/tyrian2000.zip (tyrian.hdt, tyrian1.lvl,
music.mus etc.). Veja LEIA-ME.txt.
EOF
tar -czf "dist/$NAME.tar.gz" -C dist "$NAME"

echo
echo "built: dist/$NAME.tar.gz (SDL2 linked statically)"
