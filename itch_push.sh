#!/bin/bash
# itch_push.sh — send a release package to the itch.io page with butler.
#
# Used only for GitHub releases (see .github/workflows/build.yml), after
# add_game_data.sh has put the data in the package.  Needs the BUTLER_API_KEY
# secret (an itch.io API key); without it this does nothing, so forks and
# repositories without the key still build normally.
#
# butler uploads only what changed, and the itch.io app updates installed
# copies on its own.  The channel name ("windows", "linux") also tells
# itch.io which system each file is for.
#
# Usage: ./itch_push.sh dist/Tyrian2000Plus-windows-x64 windows v1.0.2
set -euo pipefail

PKG="$1"
CHANNEL="$2"
VERSION="$3"
TARGET="gabrielbadolato/tyrian2000plus"
BUTLER_URL="https://broth.itch.zone/butler/linux-amd64/LATEST/archive/default"

if [ -z "${BUTLER_API_KEY:-}" ]; then
	echo "BUTLER_API_KEY is not set; skipping the itch.io upload"
	exit 0
fi
[ -d "$PKG" ] || { echo "no $PKG" >&2; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
curl -fsSL --retry 4 -o "$TMP/butler.zip" "$BUTLER_URL"
unzip -q "$TMP/butler.zip" -d "$TMP/butler"
chmod +x "$TMP/butler/butler"
"$TMP/butler/butler" -V

"$TMP/butler/butler" push "$PKG" "$TARGET:$CHANNEL" --userversion "$VERSION"
