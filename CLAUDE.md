# Tyrian2000 Plus

Fork pessoal do OpenTyrian2000 (Tyrian 2000 em C/SDL2), mantido por Gabriel
Badolato. Objetivos, em ordem: controles modernos (feito), build nativo para
Windows x64, melhorias de vídeo, e depois Steam Deck / Steam Machine (Linux).

O dono do projeto não é desenvolvedor de C nem usa git no dia a dia: explique
em português, de forma simples, e entregue builds prontos para testar.

## Rotina obrigatória: atualizações do upstream

Antes de começar qualquer melhoria nova, verifique se há novidades nos dois
projetos de origem e traga o que for seguro:

- `upstream`   = https://github.com/KScl/opentyrian2000 (base direta, Tyrian 2000)
- `opentyrian` = https://github.com/opentyrian/opentyrian (projeto pai)

Regras:
1. `git fetch upstream opentyrian` e liste `git log main..upstream/master` e
   `git log main..opentyrian/master`.
2. Mudanças de `upstream` (KScl): faça merge normalmente.
3. Mudanças de `opentyrian`: traga uma a uma com `cherry-pick -x`, apenas
   correções pequenas e de baixo risco. Conflitos simples podem ser resolvidos
   à mão adaptando ao código do Tyrian 2000.
4. NÃO traga reescritas grandes (saves/config, input, file I/O, renderização)
   sem antes perguntar ao Gabriel. Elas mudam formato de save e comportamento
   específico do Tyrian 2000. O próprio KScl costuma portá-las para o fork; é
   melhor esperar e recebê-las via `upstream`.
5. Depois de integrar: compile para Windows, rode `tests/test_gamepad.c` e um
   teste headless do jogo com os dados reais (SDL_VIDEODRIVER=dummy). Só então
   faça push e avise o Gabriel do que entrou e do que ficou de fora.

Histórico: em out/2026 o branch `merge-opentyrian` juntou o OpenTyrian completo
(reescritas de input, config/saves, file I/O, logging, SWAR), com aprovação do
Gabriel. Depois disso, as próximas atualizações do `opentyrian` devem ser
pequenas e podem seguir as regras acima normalmente.

## Build

- Windows x64: `./make_windows.sh` (no Linux) cross-compila com llvm-mingw +
  SDL2 2.32 e SDL2_net 2.4, baixando tudo em `build/win`, e gera
  `dist/Tyrian2000Plus-windows-x64.zip` (Tyrian2000Plus.exe + SDL2.dll +
  SDL2_net.dll + LEIA-ME + licenças). O Makefile embute ícone e manifesto
  UTF-8 de `visualc/resources.rc`.
- Linux x86_64 / Steam Deck: `./make_linux.sh` compila SDL2 estático (só
  depende da glibc) e gera `dist/Tyrian2000Plus-linux-x86_64.tar.gz`.
- GitHub Actions (`.github/workflows/build.yml`) roda os dois scripts e os
  testes a cada push; os pacotes ficam em "Artifacts" da execução e são
  anexados a uma release publicada. Os pacotes NUNCA levam os dados do jogo.
- Os dados do jogo (freeware) ficam em `data/` ao lado do executável e não
  vão para o repositório.
- Testes: `tests/run_tests.sh [pasta-do-jogo]` compila e roda
  `tests/test_gamepad.c` (controles virtuais) e, se receber uma pasta com
  data/, tyrian.sav e tyrian.cfg reais, `tests/test_saves.c` (carrega e
  regrava os saves; devem sair idênticos).
- Pastas do usuário: portátil (ao lado do exe) se existir `opentyrian.cfg` lá;
  senão `%APPDATA%/OpenTyrian2000` ou `~/.config/opentyrian2000`.

## Controles

`src/joystick.c` usa SDL_GameController quando o SDL reconhece o dispositivo
(PAD_BUTTON/PAD_AXIS, seção "gamepad" no opentyrian.cfg) e cai para a API
antiga de joystick (AXIS/BUTTON/HAT) caso contrário. Hotplug é verificado a
cada 500 ms, inclusive durante as fases.

Steam Controller (testado com o modelo 2026 via puck): fora da biblioteca da
Steam, a Steam o converte em teclado/mouse e o jogo não vê controle (sem
vibração, Y abre o teclado virtual). Adicionado como "jogo não-Steam", vira
gamepad 28de:1302 e vibra. Isso está explicado em `doc/LEIA-ME.txt`.
Pendente: jogadores com o controle 2026 sem a Steam (o jogo não será
publicado na Steam no início).

Tiro automático: opção "Auto Fire" em Setup > Controls (seção "gameplay",
`auto_fire` no opentyrian.cfg). Cada toque no tiro liga/desliga o disparo
contínuo, por jogador (`src/mainint.c`). O submenu Controls é onde entram as
próximas opções de controle.

Setup > Controls também tem Keyboard..., Controller... (com Buttons...) e
Mouse..., os mesmos ajustes do menu da nave, feitos no estilo do `setupMenu`
(`src/opentyr.c`). A captura de botão do controle fica em
`assign_joystick_action()` (`src/joystick.c`), usada pelos dois menus.

Log: o jogo grava `opentyrian.log` na pasta do usuário; peça esse arquivo
ao investigar problemas no PC do Gabriel.

Telas de ajuda: `padHelpText()` em `src/helptext.c` troca, só quando há um
controle conectado, as menções a teclas dos textos do `tyrian.hdt` (Enter, ESC,
setas, Alt-X, "[/]") pelos nomes dos botões do controle (conforme o mapeamento
atual). Texto que não reconhece fica igual.

## Steam (biblioteca)

Arte própria em pixel art (nada tirado do jogo) em `linux/steam/`: capa,
banner, fundo (hero), logo e ícone, desenhados por `gerar-arte.py` (Pillow;
os PNGs ficam no repositório). O pacote Linux leva a pasta `steam/` e
`instalar-na-steam.py`, que o `atualizar-tyrian.sh` roda: cria o atalho
não-Steam se faltar, aponta o ícone e copia as imagens para
`userdata/*/config/grid`. O shortcuts.vdf só é gravado com a Steam fechada
(o atualizador pergunta se pode fechá-la). O atualizador vai no pacote em
`atualizador/` e se instala com `mv`, para não quebrar o script em execução.

## Vídeo

Padrão: Scaling Mode "Fit 4:3" (fiel ao monitor 4:3 de 1995). A opção
"Sharp Bilinear" em Setup > Graphics (`sharp_bilinear` na seção "video",
desligada por padrão) amplia primeiro por um número inteiro (pixels nítidos)
e depois encaixa na janela com filtro bilinear, para os pixels ficarem do
mesmo tamanho quando a ampliação não é inteira (`sharp_bilinear_prescale()`
em `src/video.c`). Em múltiplos exatos nada muda.

Steam Deck: o modo "Integer" já dá o encaixe exato 4x (1280x800, pixels
quadrados), então não há opção nova para isso. O jogo não muda a taxa da tela
no Modo de Jogo (o gamescope/Steam controla); o LEIA-ME explica 70 Hz no OLED
e 60 Hz no LCD (40-60 Hz, nada combina com 35 FPS).
