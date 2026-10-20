#!/bin/bash
# make_installer.sh — build the Windows installer (windows/installer.nsi) from
# a package folder made by make_windows.sh.
#
# The installer holds exactly the files of that folder, so for a release it
# runs after add_game_data.sh and carries the game data too.  The uninstaller
# gets the same list and removes only those files.
#
# Build requirements: makensis (NSIS 3; the "nsis" package on Debian/Ubuntu),
# git (for the version).
# Usage: ./make_installer.sh dist/Tyrian2000Plus-windows-x64 dist/Tyrian2000Plus-windows-x64-setup.exe
set -euo pipefail

PKG="$(cd "$1" && pwd)"
OUT="$(realpath -m "$2")"
cd "$(dirname "$0")"

[ -f "$PKG/Tyrian2000Plus.exe" ] || { echo "no $PKG/Tyrian2000Plus.exe" >&2; exit 1; }
if ! find "$PKG/data" -maxdepth 1 -iname tyrian1.lvl | grep -q .; then
    echo "Note: $PKG/data has no game data; the installed game will ask for it."
fi

# Same version as the exe (see the Makefile): v1.2.3-4-gabcdef -> 1.2.3.0
VERSION="$( (git describe --tags || git rev-parse --short HEAD) 2>/dev/null || echo unknown)"
VERSION_NUM="$(echo "$VERSION" | sed -n 's/^v\([0-9]*\)\.\([0-9]*\)\.\{0,1\}\([0-9]*\).*/\1.\2.\3.0/p' | sed 's/\.\./.0./')"
[ -n "$VERSION_NUM" ] || VERSION_NUM="0.0.0.0"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# The file lists as two NSIS macros.  In the installed paths "$" starts a
# variable, so it is written "$$" (some data files are called newsh$.shp and
# the like); File reads the source paths as they are.
nsis() { printf '%s' "$1" | sed 's/\$/$$/g'; }
{
    echo '!macro PKG_INSTALL'
    (cd "$PKG" && find . -type d | sort) | while read -r dir; do
        rel="${dir#.}"
        echo "	SetOutPath \"\$INSTDIR$(nsis "${rel//\//\\}")\""
        (cd "$PKG/$dir" && find . -maxdepth 1 -type f | sort) | while read -r f; do
            echo "	File \"$PKG/${dir#./}/${f#./}\""
        done
    done
    echo '!macroend'
    echo '!macro PKG_UNINSTALL'
    (cd "$PKG" && find . -type f | sort) | while read -r f; do
        rel="${f#.}"
        echo "	Delete \"\$INSTDIR$(nsis "${rel//\//\\}")\""
    done
    # deepest folders first; RMDir without /r only removes empty ones
    (cd "$PKG" && find . -mindepth 1 -type d | sort -r) | while read -r dir; do
        rel="${dir#.}"
        echo "	RMDir \"\$INSTDIR$(nsis "${rel//\//\\}")\""
    done
    echo '!macroend'
} > "$TMP/files.nsh"
sed -i 's|/\./|/|g' "$TMP/files.nsh"

mkdir -p "$(dirname "$OUT")"
makensis -V2 -INPUTCHARSET UTF8 \
    -DPKG_FILES="$TMP/files.nsh" -DOUTFILE="$OUT" \
    -DVERSION="$VERSION" -DVERSION_NUM="$VERSION_NUM" \
    windows/installer.nsi

echo "built: $OUT"
