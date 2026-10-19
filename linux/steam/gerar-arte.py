#!/usr/bin/env python3
"""gerar-arte.py — desenha a arte do Tyrian2000Plus para a biblioteca da Steam.

    python3 linux/steam/gerar-arte.py [pasta-de-saida]

Arte própria em pixel art (nenhum gráfico tirado do jogo).  Tudo é desenhado
numa tela pequena e ampliado por um número inteiro, como no jogo original.
Gera, na pasta linux/steam (ou na pasta indicada):

    capa.png    600x900   capa vertical da biblioteca
    banner.png  920x430   capa horizontal (jogos recentes)
    fundo.png   3840x1240 banner do topo da página do jogo (sem logo)
    logo.png    1152x416  logo com fundo transparente (vai por cima do fundo)
    icone.png   256x256   ícone

Precisa do Pillow (pip install pillow).  Os PNGs gerados ficam no repositório,
então só é preciso rodar isto para mudar a arte.
"""
import os
import random
import sys

from PIL import Image

OUT = sys.argv[1] if len(sys.argv) > 1 else os.path.dirname(os.path.abspath(__file__))

# ---- paleta ----
PAL = {
    "k": (12, 14, 34),     # contorno
    "d": (52, 64, 92),     # casco escuro
    "m": (98, 116, 150),   # casco médio
    "l": (160, 176, 204),  # casco claro
    "w": (232, 240, 250),  # brilho
    "c": (28, 72, 168),    # cabine escura
    "C": (96, 184, 255),   # cabine clara
    "r": (204, 48, 56),    # vermelho
    "R": (120, 24, 36),    # vermelho escuro
    "o": (255, 150, 40),   # chama laranja
    "y": (255, 232, 110),  # chama amarela
    "g": (70, 200, 120),   # verde (inimigo)
    "G": (26, 98, 64),     # verde escuro
    "p": (170, 70, 210),   # roxo (inimigo)
    "P": (82, 30, 120),    # roxo escuro
}


def sprite(half_rows):
    """Linhas da metade esquerda (a última coluna é o centro) -> sprite simétrico."""
    return [r + r[-2::-1] for r in half_rows]


# Nave do jogador (desenho próprio), vista de cima, apontando para cima.
NAVE = sprite([
    "...........k",
    "..........kw",
    "..........kw",
    ".........klw",
    ".........klw",
    "........kmlC",
    "........kmcC",
    "........kmcC",
    "........kmcc",
    "...k...kdmlc",
    "..kw...kdmlw",
    "..kl...kdmlw",
    "..kr..kddmlw",
    "..kr.kmddmlw",
    "..km.kmdmmll",
    ".kdmkmmdmllw",
    ".kdmmmldmllw",
    "kdmmmlldmllw",
    "kdmmllldmmll",
    "kdmrrrldmmll",
    "kdRrrrkddmml",
    "kkkkkkkRrdmm",
    "......kRrddm",
    ".......kkkdd",
    "........kyoR",
    ".........yok",
    ".........oy.",
    "..........o.",
])

# Inimigos (desenhos próprios).
DRONE = sprite([
    "....kk",
    "..kkgg",
    ".kgGgg",
    "kgGGgw",
    "kgGkgg",
    "kgk.kG",
    ".k...k",
])
DISCO = sprite([
    "...kkkk",
    ".kkPppp",
    "kPPpppw",
    "kkkkkkk",
    ".kyky.k",
    "..kkkkk",
])


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))


class Tela:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        self.px = self.img.load()

    def ponto(self, x, y, cor):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.px[x, y] = cor + (255,) if len(cor) == 3 else cor

    def espaco(self, topo, base, semente, densidade=0.012):
        """Fundo degradê com faixas de 'dither', estrelas e algumas nebulosas."""
        bayer = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
        passos = 8
        for y in range(self.h):
            t = y / max(1, self.h - 1) * passos
            for x in range(self.w):
                n = int(t) + (1 if (t - int(t)) * 16 > bayer[y % 4][x % 4] else 0)
                self.ponto(x, y, lerp(topo, base, min(n, passos) / passos))
        rnd = random.Random(semente)
        for _ in range(int(self.w * self.h * densidade)):
            x, y = rnd.randrange(self.w), rnd.randrange(self.h)
            b = rnd.random()
            if b > 0.97:   # estrela grande em cruz
                for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
                    self.ponto(x + dx, y + dy, (255, 255, 255) if (dx, dy) == (0, 0) else (150, 170, 230))
            elif b > 0.8:
                self.ponto(x, y, (230, 236, 255))
            else:
                v = rnd.randrange(70, 150)
                self.ponto(x, y, (v, v, v + 40))

    def planeta(self, cx, cy, r, claro, escuro, aneis=True):
        """Planeta com sombreado em 'dither' e listras; anel opcional."""
        bayer = [[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                dx, dy = (x - cx) / r, (y - cy) / r
                if dx * dx + dy * dy > 1:
                    continue
                luz = 1 - ((dx + 0.55) ** 2 + (dy + 0.55) ** 2) / 2.6   # luz do alto à esquerda
                faixa = 0.12 if int((dy + 1) * 5) % 2 else 0
                nivel = max(0.0, min(1.0, luz + faixa))
                q = int(nivel * 4 * 16)
                n = q // 16 + (1 if q % 16 > bayer[y % 4][x % 4] else 0)
                self.ponto(x, y, lerp(escuro, claro, min(n, 4) / 4))
        if aneis:
            import math
            a, b = r * 1.8, r * 0.32
            for k in range(int(a * 8)):
                th = 2 * math.pi * k / int(a * 8)
                for e, cor in ((0, (232, 204, 156)), (1.2, (150, 118, 88))):
                    x = round(cx + (a + e) * math.cos(th))
                    y = round(cy + (b + e * 0.3) * math.sin(th) - math.cos(th) * r * 0.25)
                    atras = math.sin(th) < 0
                    if atras and (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                        continue   # a parte de trás do anel fica escondida
                    self.ponto(x, y, cor)

    def desenhar(self, spr, x0, y0, esc=1):
        for j, linha in enumerate(spr):
            for i, ch in enumerate(linha):
                if ch in PAL:
                    for a in range(esc):
                        for b in range(esc):
                            self.ponto(x0 + i * esc + a, y0 + j * esc + b, PAL[ch])

    def tiro(self, x, y, comp):
        for k in range(comp):
            self.ponto(x, y + k, (255, 255, 200) if k < comp // 2 else (255, 200, 60))
            self.ponto(x + 1, y + k, (255, 220, 90))

    def explosao(self, cx, cy, r, semente):
        rnd = random.Random(semente)
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5 + rnd.random() * r * 0.5
                if d < r * 0.45:
                    self.ponto(x, y, (255, 250, 200))
                elif d < r * 0.75:
                    self.ponto(x, y, (255, 190, 60))
                elif d < r and rnd.random() < 0.7:
                    self.ponto(x, y, (210, 70, 40))

    def salvar(self, nome, esc, fundo_transparente=False):
        img = self.img
        if not fundo_transparente:
            base = Image.new("RGBA", img.size, (0, 0, 0, 255))
            img = Image.alpha_composite(base, img).convert("RGB")
        img = img.resize((self.w * esc, self.h * esc), Image.NEAREST)
        img.save(os.path.join(OUT, nome), optimize=True)
        print(f"{nome}: {img.size[0]}x{img.size[1]}")


# ---- logo (fonte própria 5x7) ----
FONTE = {
    "T": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    "Y": ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    "R": ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    "I": ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"],
    "A": [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    "N": ["#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"],
    "2": [".###.", "#...#", "....#", "..##.", ".#...", "#....", "#####"],
    "0": [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "P": ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    "L": ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    "U": ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    "S": [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    " ": ["..", "..", "..", "..", "..", "..", ".."],
}


def mascara_texto(texto, grossura):
    """Texto -> conjunto de pixels; cada pixel da fonte vira grossura x grossura."""
    pts, x = set(), 0
    for ch in texto:
        g = FONTE[ch]
        for j, linha in enumerate(g):
            for i, c in enumerate(linha):
                if c == "#":
                    for a in range(grossura):
                        for b in range(grossura):
                            pts.add((x + i * grossura + a, j * grossura + b))
        x += (len(g[0]) + 1) * grossura
    return pts, x - grossura, 7 * grossura


def logo(tela, x0, y0, g=2, gg=None):
    """TYRIAN em dourado; 2000 PLUS embaixo, prata e ciano.

    g = grossura de "2000 PLUS"; gg = grossura de TYRIAN (padrão 2*g)."""
    gg = gg or g * 2
    def texto(t, x, y, gr, cima, baixo, contorno=(12, 14, 34)):
        pts, w, h = mascara_texto(t, gr)
        borda = {(px + dx, py + dy) for px, py in pts
                 for dx in (-1, 0, 1, 2) for dy in (-1, 0, 1, 2)} - pts
        for px, py in borda:                      # contorno + sombra
            tela.ponto(x + px, y + py, contorno)
        for px, py in pts:
            t_ = py / max(1, h - 1)
            cor = lerp(cima, baixo, t_)
            if py % gr == 0 and (px, py - 1) not in pts:
                cor = lerp(cor, (255, 255, 255), 0.6)   # brilho no topo
            tela.ponto(x + px, y + py, cor)
        return w, h

    _, h1 = texto("TYRIAN", x0, y0, gg, (255, 240, 150), (220, 90, 20))
    wt = mascara_texto("TYRIAN", gg)[1]
    w2 = mascara_texto("2000 PLUS", g)[1]
    xs = x0 + (wt - w2) // 2
    w20 = mascara_texto("2000 ", g)[1]
    texto("2000", xs, y0 + h1 + 3 * g, g, (240, 244, 255), (120, 136, 170))
    texto("PLUS", xs + w20 + g, y0 + h1 + 3 * g, g, (200, 250, 255), (30, 150, 230))
    return wt, h1 + 3 * g + 7 * g


def tamanho_logo(g, gg=None):
    gg = gg or g * 2
    return mascara_texto("TYRIAN", gg)[1], 7 * gg + 3 * g + 7 * g


# ---- imagens ----
def capa():
    t = Tela(150, 225)   # x4 = 600x900
    t.espaco((10, 8, 40), (40, 14, 60), 1)
    t.planeta(118, 196, 36, (230, 140, 90), (70, 24, 40))
    t.desenhar(DISCO, 10, 118, 2)
    t.desenhar(DRONE, 108, 92, 2)
    t.explosao(36, 100, 8, 3)
    t.tiro(73, 96, 8)
    t.tiro(73, 80, 8)
    t.tiro(64, 112, 6)
    t.tiro(84, 112, 6)
    t.desenhar(NAVE, 52, 132, 2)
    lw, lh = tamanho_logo(2)
    logo(t, (150 - lw) // 2, 14, 2)
    t.salvar("capa.png", 4)


def banner():
    t = Tela(184, 86)    # x5 = 920x430
    t.espaco((8, 8, 36), (36, 14, 58), 2)
    t.planeta(160, 74, 22, (230, 140, 90), (70, 24, 40), aneis=False)
    t.desenhar(NAVE, 128, 18, 2)
    t.tiro(149, 4, 8)
    t.desenhar(DRONE, 160, 4, 2)
    lw, lh = tamanho_logo(2, 3)
    logo(t, 10, (86 - lh) // 2, 2, 3)
    t.salvar("banner.png", 5)


def fundo():
    t = Tela(384, 124)   # x10 = 3840x1240
    t.espaco((6, 6, 30), (44, 16, 66), 3)
    t.planeta(300, 92, 50, (230, 140, 90), (70, 24, 40))
    t.planeta(60, 18, 9, (140, 200, 255), (20, 40, 90), aneis=False)
    t.desenhar(NAVE, 200, 48, 2)
    t.tiro(221, 30, 8)
    t.tiro(221, 14, 8)
    t.desenhar(DISCO, 250, 22, 2)
    t.desenhar(DRONE, 160, 14, 2)
    t.desenhar(DRONE, 186, 2, 2)
    t.explosao(222, 8, 5, 4)
    t.salvar("fundo.png", 10)


def logo_png():
    lw, lh = tamanho_logo(2)
    t = Tela(lw + 4, lh + 4)
    logo(t, 1, 1, 2)
    esc = 1280 // t.w
    t.salvar("logo.png", esc, fundo_transparente=True)


def icone():
    t = Tela(64, 64)     # x4 = 256x256
    t.espaco((10, 8, 40), (44, 16, 66), 5, 0.03)
    t.planeta(50, 52, 16, (230, 140, 90), (70, 24, 40), aneis=False)
    t.desenhar(NAVE, 9, 4, 2)
    t.salvar("icone.png", 4)


if __name__ == "__main__":
    capa()
    banner()
    fundo()
    logo_png()
    icone()
