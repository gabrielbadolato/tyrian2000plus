#!/bin/bash
# write_source_note.sh — write CODIGO-FONTE.txt into a package: the link to
# the source code of this exact build, which the GPL asks every copy of the
# program to come with (the itch.io page only gets the packages).
#
# Usage: ./write_source_note.sh dist/Tyrian2000Plus-windows-x64 [crlf]
#   crlf: write the note with Windows line endings
set -euo pipefail

PKG="$1"
EOL="${2:-}"
REPO="https://github.com/gabrielbadolato/tyrian2000plus"
cd "$(dirname "$0")"

# A release links to its tag, anything else to its commit.
REV=$(git describe --tags --exact-match 2>/dev/null || git rev-parse HEAD 2>/dev/null || echo main)

note() {
    cat <<NOTE
CODIGO-FONTE / SOURCE CODE
==========================

Tyrian 2000+ e software livre (GNU GPL versao 2 ou posterior, veja
licencas/GPL-2.0.txt). O codigo-fonte exato desta versao esta em:

Tyrian 2000+ is free software (GNU GPL version 2 or later, see
licencas/GPL-2.0.txt). The exact source code of this version is at:

    $REPO/tree/$REV
NOTE
}
if [ "$EOL" = crlf ]; then
    note | sed 's/$/\r/' > "$PKG/CODIGO-FONTE.txt"
else
    note > "$PKG/CODIGO-FONTE.txt"
fi
