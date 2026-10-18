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

- Windows x64: cross-compile com clang/llvm-mingw + SDL2 2.32 e SDL2_net 2.4,
  `make PLATFORM=WIN32 CC=x86_64-w64-mingw32-clang WINDRES=x86_64-w64-mingw32-windres`
  (o Makefile já embute ícone e manifesto UTF-8 de `visualc/resources.rc`).
  Entregar o exe como `Tyrian2000Plus.exe` junto com SDL2.dll e SDL2_net.dll.
- Os dados do jogo (freeware) ficam em `data/` ao lado do executável e não
  vão para o repositório.
- Testes: `tests/test_gamepad.c` (controles virtuais) e `tests/test_saves.c`
  (carrega e regrava um TYRIAN.SAV/TYRIAN.CFG reais; devem sair idênticos).
  Compile todos os `src/*.c` com `-Dmain=opentyrian_main` em opentyr.c e
  linke junto com o teste.
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

Log: o jogo grava `opentyrian.log` na pasta do usuário; peça esse arquivo
ao investigar problemas no PC do Gabriel.
