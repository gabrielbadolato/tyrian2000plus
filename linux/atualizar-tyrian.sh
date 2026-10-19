#!/bin/bash
# atualizar-tyrian.sh — instala ou atualiza o Tyrian2000Plus no Linux / Steam Deck.
#
# Baixa o build Linux mais recente que passou no GitHub Actions, instala em
# ~/Jogos/Tyrian2000Plus (mantendo dados e saves) e, na primeira vez, baixa os
# dados freeware do Tyrian 2000 do site original (camanis.net). Também põe o
# jogo na biblioteca da Steam, com ícone e arte (steam/instalar-na-steam.py).
#
# O repositório é privado, então o download usa uma chave do GitHub
# (fine-grained token, só leitura de Actions deste repositório). Ela é pedida
# na primeira vez (ou lida de um github-token.txt ao lado do script) e
# guardada em ~/.config/tyrian2000plus/github-token.
#
#   bash atualizar-tyrian.sh
set -euo pipefail

REPO="gabrielbadolato/tyrian2000plus"
ARTIFACT="Tyrian2000Plus-linux-x86_64"
DEST="${TYRIAN_DIR:-$HOME/Jogos/Tyrian2000Plus}"
CONF="$HOME/.config/tyrian2000plus"
TOKEN_FILE="$CONF/github-token"
DATA_URL="https://www.camanis.net/tyrian/tyrian2000.zip"

have_gui() { [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] && command -v kdialog >/dev/null; }
say() {
    echo "$1"
    if have_gui && [ ! -t 1 ]; then kdialog --title "Tyrian2000Plus" --msgbox "$1" || true; fi
}
confirmar() {  # pergunta sim/não; sem tela nem terminal, responde não
    if have_gui && [ ! -t 1 ]; then
        kdialog --title "Tyrian2000Plus" --yesno "$1"
    elif [ -t 0 ]; then
        read -r -p "$1 [s/N] " r; [[ "$r" =~ ^[sSyY] ]]
    else
        return 1
    fi
}
fail() {
    echo "ERRO: $1" >&2
    if have_gui && [ ! -t 1 ]; then kdialog --title "Tyrian2000Plus" --error "$1" || true; fi
    exit 1
}

# ---- Steam: atalho, ícone e arte da biblioteca ----
# Deixa em $extra uma frase para a mensagem final.
passo_steam() {
    extra=""
    [ -f "$DEST/steam/instalar-na-steam.py" ] || return 0
    local rc=0
    python3 "$DEST/steam/instalar-na-steam.py" "$DEST" || rc=$?
    case "$rc" in
    0) extra=" O jogo está na biblioteca da Steam com ícone e arte." ;;
    2) ;;   # Steam não instalada
    3)
        if [ "${XDG_CURRENT_DESKTOP:-}" != "gamescope" ] && confirmar \
            "Para pôr o Tyrian2000Plus na Steam com ícone, a Steam precisa ser fechada por um instante. Fechar a Steam agora?"; then
            steam -shutdown >/dev/null 2>&1 || true
            for _ in $(seq 60); do pgrep -x steam >/dev/null || break; sleep 1; done
            if ! pgrep -x steam >/dev/null && python3 "$DEST/steam/instalar-na-steam.py" "$DEST"; then
                extra=" O jogo está na Steam com ícone e arte: abra a Steam de novo (ou volte ao Modo de Jogo)."
            else
                extra=" A Steam não fechou; rode o atualizador de novo para pôr o ícone."
            fi
        else
            extra=" A arte da Steam foi copiada; o ícone entra quando você rodar o atualizador com a Steam fechada."
        fi ;;
    *) extra=" Não consegui pôr o jogo na Steam (rode pelo terminal para ver o erro)." ;;
    esac
}

# ---- chave do GitHub ----
mkdir -p "$CONF"
# A chave pode vir num arquivo github-token.txt ao lado do script (enviado do
# PC); ele é movido para a pasta de configuração, para não ficar em Downloads.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -s "$SCRIPT_DIR/github-token.txt" ]; then
    (umask 077; sed '1s/^\xEF\xBB\xBF//' "$SCRIPT_DIR/github-token.txt" | tr -d '[:space:]' > "$TOKEN_FILE"; echo >> "$TOKEN_FILE")
    rm -f "$SCRIPT_DIR/github-token.txt"
fi
if [ ! -s "$TOKEN_FILE" ]; then
    msg="Cole a chave do GitHub (começa com github_pat_):"
    if have_gui; then
        token=$(kdialog --title "Tyrian2000Plus" --password "$msg") || exit 1
    else
        read -r -s -p "$msg " token; echo
    fi
    [ -n "$token" ] || fail "nenhuma chave informada"
    (umask 077; printf '%s\n' "$token" > "$TOKEN_FILE")
fi
TOKEN=$(tr -d '[:space:]' < "$TOKEN_FILE")

api() {  # $1 = caminho da API
    curl -fsSL -H "Authorization: Bearer $TOKEN" \
        -H "Accept: application/vnd.github+json" "https://api.github.com/repos/$REPO/$1"
}

# ---- build mais recente que passou ----
runs=$(api "actions/workflows/build.yml/runs?status=success&event=push&per_page=1") || {
    rm -f "$TOKEN_FILE"
    fail "o GitHub recusou a chave. Rode de novo e cole uma chave válida."
}
read -r run_id run_desc < <(python3 -c '
import json, sys
r = json.load(sys.stdin)["workflow_runs"]
if not r: sys.exit(1)
print(r[0]["id"], r[0]["head_sha"][:7] + " (" + r[0]["head_branch"] + ")")
' <<<"$runs") || fail "nenhum build pronto no GitHub"

if [ -f "$DEST/.versao" ] && [ "$(cat "$DEST/.versao")" = "$run_id" ] && [ -x "$DEST/Tyrian2000Plus" ]; then
    passo_steam
    say "O Tyrian2000Plus já está na versão mais nova ($run_desc).$extra"
    exit 0
fi

url=$(api "actions/runs/$run_id/artifacts" | python3 -c '
import json, sys
for a in json.load(sys.stdin)["artifacts"]:
    if a["name"] == sys.argv[1] and not a["expired"]:
        print(a["archive_download_url"]); break
' "$ARTIFACT")
[ -n "$url" ] || fail "o build $run_desc não tem o pacote Linux"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "Baixando a versão $run_desc ..."
curl -fsSL -H "Authorization: Bearer $TOKEN" -o "$TMP/artifact.zip" "$url"
python3 -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' \
    "$TMP/artifact.zip" "$TMP/x"
tar -xzf "$TMP/x/$ARTIFACT.tar.gz" -C "$TMP/x"

# Copia o pacote por cima da instalação: dados, saves e configurações
# não fazem parte do pacote, então ficam como estão.
mkdir -p "$DEST"
# O atualizador novo do pacote entra por "mv" (arquivo novo): este script pode
# estar rodando de $DEST, e o bash lê o arquivo aos poucos enquanto roda.
mv "$TMP/x/$ARTIFACT/atualizador/atualizar-tyrian.sh" "$TMP/atualizar-tyrian.sh"
rmdir "$TMP/x/$ARTIFACT/atualizador"
cp -r "$TMP/x/$ARTIFACT/." "$DEST/"
chmod +x "$TMP/atualizar-tyrian.sh"
mv -f "$TMP/atualizar-tyrian.sh" "$DEST/atualizar-tyrian.sh"
chmod +x "$DEST/Tyrian2000Plus"

# ---- dados do jogo (só na primeira vez) ----
if ! find "$DEST/data" -maxdepth 1 -iname "tyrian1.lvl" | grep -q .; then
    echo "Baixando os dados do Tyrian 2000 (freeware) de $DATA_URL ..."
    curl -fsSL -o "$TMP/tyrian2000.zip" "$DATA_URL"
    python3 - "$TMP/tyrian2000.zip" "$DEST/data" <<'EOF'
import os, sys, zipfile
z = zipfile.ZipFile(sys.argv[1])
names = [n for n in z.namelist() if not n.endswith("/")]
probe = [n for n in names if os.path.basename(n).lower() == "tyrian1.lvl"]
if not probe:
    sys.exit("o zip não tem os dados esperados")
folder = os.path.dirname(probe[0])
for n in names:
    if os.path.dirname(n) == folder:
        with open(os.path.join(sys.argv[2], os.path.basename(n).lower()), "wb") as f:
            f.write(z.read(n))
EOF
fi

echo "$run_id" > "$DEST/.versao"

# ---- atalhos no menu do Modo Desktop ----
apps="$HOME/.local/share/applications"
mkdir -p "$apps"
cat > "$apps/tyrian2000plus.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Tyrian2000Plus
Exec="$DEST/Tyrian2000Plus"
Path=$DEST
Icon=$DEST/Tyrian2000Plus.png
Categories=Game;
EOF
cat > "$apps/atualizar-tyrian2000plus.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Atualizar Tyrian2000Plus
Exec=bash "$DEST/atualizar-tyrian.sh"
Icon=$DEST/Tyrian2000Plus.png
Categories=Game;
EOF

passo_steam
say "Tyrian2000Plus instalado na versão $run_desc, em $DEST.$extra"
