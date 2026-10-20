#!/bin/bash
# add_game_data.sh — put the freeware Tyrian 2000 data into a release package.
#
# Used only for releases (see .github/workflows/build.yml); the per-push
# builds stay without data.  The data is downloaded from the official mirror
# and copied UNCHANGED (only the file names are lowercased, as the Steam Deck
# updater does), next to a note with the license, the credits and the notice
# that this is an unofficial fan project.  The original DOS programs in the
# zip (tyrian2k.exe, setup.exe etc.) are left out: the game never opens them,
# they do not run on modern Windows, and antivirus tools may flag them.
#
# Usage: ./add_game_data.sh dist/Tyrian2000Plus-windows-x64 [crlf]
#   crlf: write the note with Windows line endings
set -euo pipefail

PKG="$1"
EOL="${2:-}"
DATA_URL="https://www.camanis.net/tyrian/tyrian2000.zip"
# sha256 of that zip: the package promises the original data, unchanged, so a
# different file stops the release instead of being shipped.  The Steam Deck
# updater (linux/atualizar-tyrian.sh) checks the same value.
DATA_SHA256="348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667"

[ -d "$PKG/data" ] || { echo "no $PKG/data" >&2; exit 1; }

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
curl -fsSL --retry 4 -o "$TMP/tyrian2000.zip" "$DATA_URL"
got=$(sha256sum "$TMP/tyrian2000.zip" | cut -d' ' -f1)
if [ "$got" != "$DATA_SHA256" ]; then
    echo "ERROR: $DATA_URL has sha256 $got, expected $DATA_SHA256" >&2
    exit 1
fi

python3 - "$TMP/tyrian2000.zip" "$PKG/data" <<'EOF'
import os, sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
names = [n for n in z.namelist() if not n.endswith("/")]
probe = [n for n in names if os.path.basename(n).lower() == "tyrian1.lvl"]
if not probe:
    sys.exit("the zip does not have the expected data")
folder = os.path.dirname(probe[0])
# the DOS programs and their helpers; the game reads none of them
skip = (".exe", ".com", ".bat", ".pif", ".ovl")
count = 0
for n in names:
    if os.path.dirname(n) == folder and not n.lower().endswith(skip):
        with open(os.path.join(sys.argv[2], os.path.basename(n).lower()), "wb") as f:
            f.write(z.read(n))
        count += 1
print(f"copied {count} data files")
EOF

rm -f "$PKG/data/COLOQUE-OS-DADOS-AQUI.txt"
note() {
    cat <<'EOF'
DADOS DO TYRIAN 2000 / TYRIAN 2000 GAME DATA
============================================

Os arquivos desta pasta sao os dados originais do Tyrian 2000, sem nenhuma
alteracao, baixados de https://www.camanis.net/tyrian/tyrian2000.zip.
Tyrian foi criado pela Eclipse Software e publicado pela Epic MegaGames em
1995. Em 2004, Jason Emery liberou o jogo como freeware nestes termos:

    "Feel free to play it all you want and share it with friends."

Ficaram de fora apenas os programas do DOS (tyrian2k.exe, setup.exe etc.),
que o Tyrian 2000+ nao usa e nao rodam no Windows atual.

Tyrian 2000+ e um projeto de fa, sem fins lucrativos e nao oficial. Ele e
sempre distribuido de graca. Ver licencas/tyrian-freeware-license.txt.

The files in this folder are the original Tyrian 2000 data, unmodified,
downloaded from https://www.camanis.net/tyrian/tyrian2000.zip. Tyrian was
developed by Eclipse Software and published by Epic MegaGames in 1995. In
2004, Jason Emery released it as freeware under the terms above. Only the DOS
programs (tyrian2k.exe, setup.exe etc.) are left out: Tyrian 2000+ does not
use them and they do not run on modern Windows. Tyrian 2000+
is an unofficial, non-commercial fan project and is always free.
EOF
}
if [ "$EOL" = crlf ]; then
    note | sed 's/$/\r/' > "$PKG/data/LEIA-ME-DADOS.txt"
else
    note > "$PKG/data/LEIA-ME-DADOS.txt"
fi
