#!/bin/bash
# atualizar-tyrian.sh — instala ou atualiza o Tyrian2000Plus no Linux / Steam Deck.
#
# Baixa a versão mais recente publicada em Releases no GitHub, instala em
# ~/Jogos/Tyrian2000Plus (mantendo saves e configurações) e, se os dados do
# Tyrian 2000 ainda não estiverem lá, baixa-os do site original (camanis.net).
# Na primeira vez, também põe o jogo na biblioteca da Steam (como jogo
# não-Steam), para abrir no Modo de Jogo; para isso a Steam fecha e abre de
# novo sozinha, se o jogador concordar.
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

# O script inteiro fica entre { e }: assim o bash lê tudo antes de começar. A
# atualização grava a versão nova deste arquivo por cima dele; sem isso, o
# bash continuaria lendo o arquivo novo do ponto em que estava no antigo.
{

REPO="gabrielbadolato/tyrian2000plus"
ARTIFACT="Tyrian2000Plus-linux-x86_64"
DEST="${TYRIAN_DIR:-$HOME/Jogos/Tyrian2000Plus}"
CONF="$HOME/.config/tyrian2000plus"
TOKEN_FILE="$CONF/github-token"
DATA_URL="https://www.camanis.net/tyrian/tyrian2000.zip"
DATA_SHA256="348bc76e73514e452279b8730cf217daf0f70a282f07b6b94af653d87e921667"

have_gui() { [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ] && command -v kdialog >/dev/null; }
ask() {  # pergunta sim/não; sem terminal nem janela, a resposta é não
    if [ -t 0 ] && [ -t 1 ]; then
        local r
        read -r -p "$1 [S/n] " r || return 1
        case "$r" in [nN]*) return 1 ;; esac
        return 0
    fi
    have_gui && kdialog --title "Tyrian 2000+" --yesno "$1"
}
say() {
    echo "$1"
    if have_gui && [ ! -t 1 ]; then kdialog --title "Tyrian 2000+" --msgbox "$1" || true; fi
}
fail() {
    echo "ERRO: $1" >&2
    if have_gui && [ ! -t 1 ]; then kdialog --title "Tyrian 2000+" --error "$1" || true; fi
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
' <<<"$rel") || fail "a última versão publicada ainda não tem o pacote Linux (logo depois de publicada, ela leva alguns minutos para ficar pronta); tente de novo daqui a pouco"
    run_desc="$versao"
fi

# ---- atalho na biblioteca da Steam (jogo não-Steam) ----
# A Steam guarda os jogos não-Steam em userdata/<conta>/config/shortcuts.vdf
# (um arquivo binário) e só lê esse arquivo ao abrir; aberta, ela regravaria
# o arquivo por cima. Por isso a Steam precisa estar fechada para a mudança.
steam_msg=""
steam_shortcuts() {  # $1 = check | add
    python3 - "$1" "$DEST" "$HOME/.local/share/Steam" "$HOME/.steam/steam" <<'EOF'
import os, struct, sys, zlib

mode, dest = sys.argv[1], sys.argv[2]
exe = os.path.join(dest, "Tyrian2000Plus")
name = "Tyrian 2000+"

def read_str(b, i):
    j = b.index(b"\0", i)
    return b[i:j], j + 1

def read_map(b, i):
    items = []  # (tipo, chave, valor), na ordem do arquivo
    while True:
        t = b[i]; i += 1
        if t == 8:
            return items, i
        key, i = read_str(b, i)
        if t == 0:
            v, i = read_map(b, i)
        elif t == 1:
            v, i = read_str(b, i)
        elif t in (2, 3):
            v, i = b[i:i + 4], i + 4
        elif t == 7:
            v, i = b[i:i + 8], i + 8
        else:
            raise ValueError("tipo %d desconhecido" % t)
        items.append((t, key, v))

def write_map(items):
    out = b""
    for t, key, v in items:
        out += bytes([t]) + key + b"\0"
        out += write_map(v) if t == 0 else (v + b"\0" if t == 1 else v)
    return out + b"\x08"

def ours(entry):  # já tem um atalho do Tyrian 2000+ (deste script ou feito à mão)
    for t, key, v in entry:
        if t == 1 and key.lower() in (b"exe", b"appname") and b"tyrian" in v.lower():
            return True
    return False

users = []
for root in sys.argv[3:]:
    ud = os.path.join(root, "userdata")
    if os.path.isdir(ud):
        users = [os.path.join(ud, u) for u in sorted(os.listdir(ud)) if u.isdigit() and u != "0"]
        break

pending = []
for u in users:
    path = os.path.join(u, "config", "shortcuts.vdf")
    if os.path.exists(path):
        data = open(path, "rb").read()
        root, end = read_map(data, 0)
        lists = [v for t, k, v in root if t == 0 and k.lower() == b"shortcuts"]
        if not lists:
            raise ValueError(path + ": sem a lista de atalhos")
        shortcuts = lists[0]
    else:
        root = [(0, b"shortcuts", [])]
        shortcuts = root[0][2]
    if not any(t == 0 and ours(v) for t, k, v in shortcuts):
        pending.append((path, root, shortcuts))

if mode == "check":
    sys.exit(0 if pending else 1)

def s(k, v): return (1, k.encode(), v.encode())
def n(k, v): return (2, k.encode(), struct.pack("<I", v))
# O mesmo número que a Steam e outros programas dão ao atalho.
appid = zlib.crc32(('"%s"%s' % (exe, name)).encode()) | 0x80000000
for path, root, shortcuts in pending:
    index = max([int(k) for t, k, v in shortcuts if k.isdigit()] + [-1]) + 1
    shortcuts.append((0, str(index).encode(), [
        n("appid", appid), s("AppName", name), s("Exe", '"%s"' % exe),
        s("StartDir", '"%s/"' % dest), s("icon", os.path.join(dest, "Tyrian2000Plus.png")),
        s("ShortcutPath", ""), s("LaunchOptions", ""), n("IsHidden", 0),
        n("AllowDesktopConfig", 1), n("AllowOverlay", 1), n("OpenVR", 0),
        n("Devkit", 0), s("DevkitGameID", ""), n("DevkitOverrideAppID", 0),
        n("LastPlayTime", 0), s("FlatpakAppID", ""), (0, b"tags", []),
    ]))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if os.path.exists(path) and not os.path.exists(path + ".antes-tyrian"):
        os.rename(path, path + ".antes-tyrian")  # cópia do original, por segurança
    with open(path + ".novo", "wb") as f:
        f.write(write_map(root))
    os.replace(path + ".novo", path)
print(len(pending))
EOF
}
steam_running() { pgrep -x steam >/dev/null 2>&1; }

add_to_steam() {  # põe o jogo na Steam, se faltar; o resultado fica em $steam_msg
    if command -v python3 >/dev/null && steam_shortcuts check 2>/dev/null; then
        if [ -n "${SteamAppId:-}${SteamGameId:-}" ] || [ "${XDG_CURRENT_DESKTOP:-}" = gamescope ]; then
            # Aberto pela própria Steam ou no Modo de Jogo: fechá-la encerraria tudo.
            steam_msg="Para pôr o jogo na biblioteca da Steam, rode este script de novo no Modo Desktop."
        elif ! steam_running || ask "Para pôr o Tyrian 2000+ na biblioteca da Steam, a Steam precisa fechar por alguns segundos (ela abre de novo sozinha). Fechar a Steam agora?"; then
            reopen=""
            if steam_running; then
                echo "Fechando a Steam ..."
                steam -shutdown >/dev/null 2>&1 || true
                for _ in $(seq 60); do steam_running || break; sleep 1; done
                reopen=1
            fi
            if steam_running; then
                steam_msg="A Steam não fechou; o jogo não foi posto na biblioteca. Rode o script de novo com a Steam fechada."
            elif steam_shortcuts add >/dev/null; then
                steam_msg="O jogo está na biblioteca da Steam, como \"Tyrian 2000+\"."
            else
                steam_msg="Não consegui pôr o jogo na biblioteca da Steam; adicione-o lá como jogo não-Steam."
            fi
            if [ -n "$reopen" ]; then
                (setsid steam >/dev/null 2>&1 &)
            fi
        else
            steam_msg="O jogo não foi posto na biblioteca da Steam. Rode o script de novo quando quiser."
        fi
    fi
}

if [ -f "$DEST/.versao" ] && [ "$(cat "$DEST/.versao")" = "$versao" ] && [ -x "$DEST/Tyrian2000Plus" ]; then
    add_to_steam
    say "O Tyrian 2000+ já está na versão mais nova ($run_desc).${steam_msg:+ $steam_msg}"
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
    # Confere o pacote com a lista SHA256SUMS publicada junto. Só as versões
    # até a v1.0.3 não têm a lista; nas outras, faltar quer dizer que a versão
    # acabou de ser publicada e a lista ainda não foi anexada.
    case "$versao" in
        v1.0|v1.0.1|v1.0.2|v1.0.3) ;;
        *) [ "$sums" != "-" ] || fail "a versão $versao ainda está sendo preparada no GitHub; tente de novo daqui a alguns minutos" ;;
    esac
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
chmod +x "$DEST/Tyrian2000Plus"
[ ! -f "$DEST/atualizar-tyrian.sh" ] || chmod +x "$DEST/atualizar-tyrian.sh"

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
Name=Tyrian 2000+
Exec="$DEST/Tyrian2000Plus"
Path=$DEST
Icon=$DEST/Tyrian2000Plus.png
Categories=Game;
EOF
cat > "$apps/atualizar-tyrian2000plus.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Atualizar Tyrian 2000+
Exec=bash "$DEST/atualizar-tyrian.sh"
Icon=$DEST/Tyrian2000Plus.png
Categories=Game;
EOF


add_to_steam
say "Tyrian 2000+ instalado na versão $run_desc, em $DEST.${steam_msg:+ $steam_msg}"
exit
}
