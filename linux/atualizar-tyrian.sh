#!/bin/bash
# atualizar-tyrian.sh — instala ou atualiza o Tyrian2000Plus no Linux / Steam Deck.
#
# Baixa a versão mais recente publicada em Releases no GitHub, instala em
# ~/Jogos/Tyrian2000Plus (mantendo saves e configurações) e, se os dados do
# Tyrian 2000 ainda não estiverem lá, baixa-os do site original (camanis.net).
#
# Para testar builds novos antes de virarem versão: guarde uma chave do GitHub
# (fine-grained token, só leitura de Actions deste repositório) em
# ~/.config/tyrian2000plus/github-token, ou num github-token.txt ao lado do
# script. Com a chave, o script pega o build mais recente que passou no
# GitHub Actions no branch main em vez da última versão publicada.
#
# Tudo o que é baixado é conferido antes de instalar: o pacote pela lista
# SHA256SUMS da versão (ou pelo código que o GitHub guarda do build) e os
# dados do jogo pelo código fixo abaixo, o mesmo de add_game_data.sh.
#
#   bash atualizar-tyrian.sh
set -euo pipefail

REPO="gabrielbadolato/tyrian2000plus"
ARTIFACT="Tyrian2000Plus-linux-x86_64"
DEST="${TYRIAN_DIR:-$HOME/Jogos/Tyrian2000Plus}"
CONF="$HOME/.config/tyrian2000plus"
TOKEN_FILE="$CONF/github-token"
DATA_URL="https://www.camanis.net/tyrian/tyrian2000.zip"
DATA_SHA256="348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667"

have_gui() { [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] && command -v kdialog >/dev/null; }
say() {
    echo "$1"
    if have_gui && [ ! -t 1 ]; then kdialog --title "Tyrian2000Plus" --msgbox "$1" || true; fi
}
fail() {
    echo "ERRO: $1" >&2
    if have_gui && [ ! -t 1 ]; then kdialog --title "Tyrian2000Plus" --error "$1" || true; fi
    exit 1
}

# ---- chave do GitHub (opcional, só para testar builds) ----
mkdir -p "$CONF"
# A chave pode vir num arquivo github-token.txt ao lado do script (enviado do
# PC); ele é movido para a pasta de configuração, para não ficar em Downloads.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -s "$SCRIPT_DIR/github-token.txt" ]; then
    (umask 077; sed '1s/^\xEF\xBB\xBF//' "$SCRIPT_DIR/github-token.txt" | tr -d '[:space:]' > "$TOKEN_FILE"; echo >> "$TOKEN_FILE")
    rm -f "$SCRIPT_DIR/github-token.txt"
fi
TOKEN=""
[ -s "$TOKEN_FILE" ] && TOKEN=$(tr -d '[:space:]' < "$TOKEN_FILE")

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
mkdir -p "$TMP/x"

# Resposta em $TMP/api.json; devolve o código HTTP (000 = sem conexão).
api() {  # $1 = caminho da API
    curl -sSL -o "$TMP/api.json" -w '%{http_code}' ${TOKEN:+-H "Authorization: Bearer $TOKEN"} \
        -H "Accept: application/vnd.github+json" "https://api.github.com/repos/$REPO/$1" || true
}
api_ok() {  # $1 = caminho da API; falha com uma mensagem clara
    local code
    code=$(api "$1")
    case "$code" in
        200) cat "$TMP/api.json" ;;
        000) fail "não consegui falar com o GitHub (sem internet?)" ;;
        401)
            # Só aqui a chave é mesmo inválida (vencida ou apagada no GitHub).
            if [ -n "$TOKEN" ]; then
                rm -f "$TOKEN_FILE"
                fail "o GitHub recusou a chave (apagada). Rode de novo para usar a versão publicada."
            fi
            fail "o GitHub recusou o pedido (código 401)" ;;
        *) fail "o GitHub respondeu com o código $code (a chave pode estar sem permissão, ou o limite de consultas acabou)" ;;
    esac
}
sha256_ok() {  # $1 = arquivo, $2 = sha256 esperado
    [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ]
}


if [ -n "$TOKEN" ]; then
    # ---- build mais recente do main que passou (para testes) ----
    # Só o main: builds de outros branches (PRs em teste) não chegam ao Deck.
    runs=$(api_ok "actions/workflows/build.yml/runs?branch=main&status=success&event=push&per_page=1")
    read -r versao run_desc < <(python3 -c '
import json, sys
r = json.load(sys.stdin)["workflow_runs"]
if not r: sys.exit(1)
print(r[0]["id"], r[0]["head_sha"][:7] + " (" + r[0]["head_branch"] + ")")
' <<<"$runs") || fail "nenhum build pronto no GitHub"
else
    # ---- última versão publicada (Releases) ----
    rel=$(api_ok "releases/latest")
    read -r versao url pacote sums < <(python3 -c '
import json, sys
r = json.load(sys.stdin)
assets = {a["name"]: a["browser_download_url"] for a in r["assets"]}
for name, link in assets.items():
    if name.endswith("-linux-x86_64.tar.gz"):
        print(r["tag_name"], link, name, assets.get("SHA256SUMS", "-")); break
else:
    sys.exit(1)
' <<<"$rel") || fail "a última versão publicada não tem o pacote Linux"
    run_desc="$versao"
fi

if [ -f "$DEST/.versao" ] && [ "$(cat "$DEST/.versao")" = "$versao" ] && [ -x "$DEST/Tyrian2000Plus" ]; then
    say "O Tyrian2000Plus já está na versão mais nova ($run_desc)."
    exit 0
fi

echo "Baixando a versão $run_desc ..."
if [ -n "$TOKEN" ]; then
    arts=$(api_ok "actions/runs/$versao/artifacts")
    read -r url digest < <(python3 -c '
import json, sys
for a in json.load(sys.stdin)["artifacts"]:
    if a["name"] == sys.argv[1] and not a["expired"]:
        print(a["archive_download_url"], (a.get("digest") or "-").removeprefix("sha256:")); break
' "$ARTIFACT" <<<"$arts") || true
    [ -n "${url:-}" ] || fail "o build $run_desc não tem o pacote Linux"
    curl -fsSL -H "Authorization: Bearer $TOKEN" -o "$TMP/artifact.zip" "$url" \
        || fail "não consegui baixar o build $run_desc (sem internet, ou a chave não tem permissão?)"
    # O GitHub guarda o sha256 de cada build; confere se chegou inteiro.
    if [ "$digest" != "-" ]; then
        sha256_ok "$TMP/artifact.zip" "$digest" || fail "o build baixado não confere com o do GitHub; nada foi instalado"
    fi
    python3 -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' \
        "$TMP/artifact.zip" "$TMP/x"
    tar -xzf "$TMP/x/$ARTIFACT.tar.gz" -C "$TMP/x"
else
    curl -fsSL -o "$TMP/pacote.tar.gz" "$url" || fail "não consegui baixar a versão $versao (sem internet?)"
    # Confere o pacote com a lista SHA256SUMS publicada junto (versões até a
    # v1.0.3 não têm a lista).
    if [ "$sums" != "-" ]; then
        curl -fsSL -o "$TMP/SHA256SUMS" "$sums" || fail "não consegui baixar a lista SHA256SUMS (sem internet?)"
        esperado=$(awk -v n="$pacote" '$2 == n || $2 == "*" n { print $1 }' "$TMP/SHA256SUMS")
        [ -n "$esperado" ] && sha256_ok "$TMP/pacote.tar.gz" "$esperado" \
            || fail "o pacote baixado não confere com a lista SHA256SUMS; nada foi instalado"
    else
        echo "Aviso: esta versão não tem a lista SHA256SUMS; o pacote não foi conferido."
    fi
    tar -xzf "$TMP/pacote.tar.gz" -C "$TMP/x"
fi

# Copia o pacote por cima da instalação: dados, saves e configurações
# não fazem parte do pacote, então ficam como estão.
mkdir -p "$DEST"
cp -r "$TMP/x/$ARTIFACT/." "$DEST/"
[ -f "$TMP/x/$ARTIFACT/atualizar-tyrian.sh" ] || cp "$0" "$DEST/atualizar-tyrian.sh" 2>/dev/null || true
chmod +x "$DEST/Tyrian2000Plus" "$DEST/atualizar-tyrian.sh"

# ---- dados do jogo (só na primeira vez) ----
if ! find "$DEST/data" -maxdepth 1 -iname "tyrian1.lvl" | grep -q .; then
    echo "Baixando os dados do Tyrian 2000 (freeware) de $DATA_URL ..."
    curl -fsSL -o "$TMP/tyrian2000.zip" "$DATA_URL" || fail "não consegui baixar os dados do jogo (sem internet?)"
    sha256_ok "$TMP/tyrian2000.zip" "$DATA_SHA256" \
        || fail "os dados baixados de $DATA_URL não são os originais esperados; nada foi instalado"
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

echo "$versao" > "$DEST/.versao"

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

say "Tyrian2000Plus instalado na versão $run_desc, em $DEST."
