#!/usr/bin/env python3
"""instalar-na-steam.py — põe o Tyrian2000Plus na biblioteca da Steam com arte.

    python3 instalar-na-steam.py PASTA-DO-JOGO

Chamado pelo atualizar-tyrian.sh.  Para cada usuário da Steam neste
computador:

  * se ainda não houver um atalho para o Tyrian2000Plus (jogo não-Steam),
    cria um em shortcuts.vdf;
  * aponta o ícone do atalho para PASTA-DO-JOGO/steam/icone.png;
  * copia a capa, o banner, o fundo e o logo para a pasta "grid" da Steam.

A Steam guarda os atalhos na memória e regrava o shortcuts.vdf ao fechar,
então ele só é alterado com a Steam fechada.  Com ela aberta, as imagens são
copiadas assim mesmo e o programa sai com código 3 se ainda faltar mudar o
shortcuts.vdf (o atualizar-tyrian.sh então oferece fechar a Steam).

Imagens que o usuário trocou por outras na Steam não são substituídas: só é
sobrescrito o que este programa mesmo instalou antes.

Códigos de saída: 0 tudo pronto, 2 Steam não encontrada, 3 falta fechar a Steam.
"""
import glob
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import zlib

NOME = "Tyrian2000Plus"
HOME = os.path.expanduser("~")
REGISTRO = os.path.join(HOME, ".config", "tyrian2000plus", "arte-steam.sha256")

# imagem do pacote -> sufixo do arquivo na pasta grid (antes vem o appid)
GRID = {"capa.png": "p.png", "banner.png": ".png", "fundo.png": "_hero.png",
        "logo.png": "_logo.png", "icone.png": "_icon.png"}

# ---- VDF binário (formato do shortcuts.vdf) ----
MAPA, TEXTO, INT32, FIM = 0, 1, 2, 8


def ler_vdf(dados, pos=0):
    """Lê um mapa até o byte FIM.  Devolve (lista de (chave, valor), pos)."""
    itens = []
    while True:
        tipo = dados[pos]
        pos += 1
        if tipo == FIM:
            return itens, pos
        fim = dados.index(b"\0", pos)
        chave = dados[pos:fim].decode("utf-8", "replace")
        pos = fim + 1
        if tipo == MAPA:
            valor, pos = ler_vdf(dados, pos)
        elif tipo == TEXTO:
            fim = dados.index(b"\0", pos)
            valor = dados[pos:fim].decode("utf-8", "replace")
            pos = fim + 1
        elif tipo == INT32:
            valor = struct.unpack_from("<i", dados, pos)[0]
            pos += 4
        else:
            raise ValueError(f"tipo VDF desconhecido {tipo}")
        itens.append((chave, valor))


def escrever_vdf(itens):
    saida = bytearray()
    for chave, valor in itens:
        nome = chave.encode() + b"\0"
        if isinstance(valor, list):
            saida += bytes([MAPA]) + nome + escrever_vdf(valor)
        elif isinstance(valor, int):
            saida += bytes([INT32]) + nome + struct.pack("<i", valor)
        else:
            saida += bytes([TEXTO]) + nome + valor.encode() + b"\0"
    return bytes(saida + bytes([FIM]))


def campo(entrada, nome, padrao=None):
    for chave, valor in entrada:
        if chave.lower() == nome.lower():
            return valor
    return padrao


def mudar(entrada, nome, valor):
    for i, (chave, _) in enumerate(entrada):
        if chave.lower() == nome.lower():
            entrada[i] = (chave, valor)
            return
    entrada.append((nome, valor))


def appid(entrada):
    """appid sem sinal (o nome dos arquivos na pasta grid)."""
    v = campo(entrada, "appid")
    if v:
        return v & 0xFFFFFFFF
    # atalhos antigos, sem appid gravado: a Steam calcula assim
    chave = (campo(entrada, "Exe", "") + campo(entrada, "AppName", "")).encode()
    return zlib.crc32(chave) | 0x80000000


def eh_tyrian(entrada):
    exe = campo(entrada, "Exe", "").strip('"').rstrip("/")
    return os.path.basename(exe).lower() == NOME.lower()


def novo_atalho(jogo, icone):
    exe = f'"{jogo}/{NOME}"'
    aid = zlib.crc32((exe + NOME).encode()) | 0x80000000
    return [
        ("appid", struct.unpack("<i", struct.pack("<I", aid))[0]),
        ("AppName", NOME), ("Exe", exe), ("StartDir", f'"{jogo}/"'),
        ("icon", icone), ("ShortcutPath", ""), ("LaunchOptions", ""),
        ("IsHidden", 0), ("AllowDesktopConfig", 1), ("AllowOverlay", 1),
        ("OpenVR", 0), ("Devkit", 0), ("DevkitGameID", ""),
        ("DevkitOverrideAppID", 0), ("LastPlayTime", 0), ("FlatpakAppID", ""),
        ("tags", []),
    ]


# ---- arquivos ----
def sha(caminho):
    with open(caminho, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def steam_aberta():
    return subprocess.run(["pgrep", "-x", "steam"], capture_output=True).returncode == 0


def pastas_de_usuario():
    raizes = [os.path.join(HOME, ".local/share/Steam"), os.path.join(HOME, ".steam/steam"),
              os.path.join(HOME, ".var/app/com.valvesoftware.Steam/.local/share/Steam")]
    vistas = set()
    for raiz in raizes:
        raiz = os.path.realpath(raiz)
        if raiz in vistas or not os.path.isdir(raiz):
            continue
        vistas.add(raiz)
        for u in sorted(glob.glob(os.path.join(raiz, "userdata", "*", "config"))):
            if os.path.basename(os.path.dirname(u)).isdigit() and os.path.basename(os.path.dirname(u)) != "0":
                yield u


def main():
    jogo = os.path.realpath(sys.argv[1])
    arte = os.path.join(jogo, "steam")
    icone = os.path.join(arte, "icone.png")

    usuarios = list(pastas_de_usuario())
    if not usuarios:
        print("Steam não encontrada; a arte não foi instalada.")
        return 2

    try:
        conhecidos = set(open(REGISTRO).read().split())
    except OSError:
        conhecidos = set()
    for img in GRID:
        conhecidos.add(sha(os.path.join(arte, img)))

    pendente = False
    for config in usuarios:
        caminho = os.path.join(config, "shortcuts.vdf")
        if os.path.exists(caminho):
            with open(caminho, "rb") as f:
                dados = f.read()
            raiz, _ = ler_vdf(dados)
            antes = escrever_vdf(raiz)   # compara pelo conteúdo, não pelos bytes
            atalhos = campo(raiz, "shortcuts")
            if atalhos is None:
                raise SystemExit(f"{caminho}: formato inesperado, nada foi alterado")
        else:
            dados = antes = None
            atalhos = []
            raiz = [("shortcuts", atalhos)]

        nossos = [a for _, a in atalhos if eh_tyrian(a)]
        if not nossos:
            nossos = [novo_atalho(jogo, icone)]
            atalhos.append((str(len(atalhos)), nossos[0]))
        for a in nossos:
            if campo(a, "icon", "") != icone:
                mudar(a, "icon", icone)

        novo = escrever_vdf(raiz)
        if novo != antes:
            if steam_aberta():
                pendente = True
            else:
                if dados is not None:
                    shutil.copyfile(caminho, caminho + ".antes-do-tyrian")
                with open(caminho + ".tmp", "wb") as f:
                    f.write(novo)
                os.replace(caminho + ".tmp", caminho)
                print(f"Atalho do {NOME} gravado em {caminho}")

        grid = os.path.join(config, "grid")
        os.makedirs(grid, exist_ok=True)
        for a in nossos:
            aid = appid(a)
            for img, sufixo in GRID.items():
                destino = os.path.join(grid, f"{aid}{sufixo}")
                if os.path.exists(destino) and sha(destino) not in conhecidos:
                    print(f"{destino}: imagem escolhida pelo usuário, mantida")
                    continue
                shutil.copyfile(os.path.join(arte, img), destino)
        print(f"Arte instalada em {grid}")

    os.makedirs(os.path.dirname(REGISTRO), exist_ok=True)
    with open(REGISTRO, "w") as f:
        f.write("\n".join(sorted(conhecidos)) + "\n")
    return 3 if pendente else 0


if __name__ == "__main__":
    sys.exit(main())
