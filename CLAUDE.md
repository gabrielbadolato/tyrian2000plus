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

Não integrado de propósito (opentyrian, out/2026): as reescritas grandes de
input/config/saves/file I/O e a otimização SWAR do backgrnd.c (624e2f2), pois
não há como validar visualmente e o ganho é irrelevante em 320x200.

## Build

- Windows x64: cross-compile com clang/llvm-mingw + SDL2 2.32 e SDL2_net 2.4,
  `make PLATFORM=WIN32 CC=x86_64-w64-mingw32-clang TARGET=opentyrian2000.exe`
  com `visualc/resources.rc` compilado via windres e passado em LDLIBS.
  Entregar o exe como `Tyrian2000Plus.exe` junto com SDL2.dll e SDL2_net.dll.
- Os dados do jogo (freeware) ficam em `data/` ao lado do executável e não
  vão para o repositório.
- O assert em `read_encrypted_pascal_string` (helptext.c) dispara com os dados
  do Tyrian 2000 em builds de depuração; é problema antigo do upstream e não
  afeta o build de release (NDEBUG).

## Controles

`src/joystick.c` usa SDL_GameController quando o SDL reconhece o dispositivo
(PAD_BUTTON/PAD_AXIS, seção "gamepad" no opentyrian.cfg) e cai para a API
antiga de joystick (AXIS/BUTTON/HAT) caso contrário. Hotplug é verificado a
cada 500 ms, inclusive durante as fases.
