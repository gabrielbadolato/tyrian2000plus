# Tyrian2000 Plus

Fork pessoal do OpenTyrian2000 (Tyrian 2000 em C/SDL2), mantido por Gabriel
Badolato. Objetivos, em ordem: controles modernos (feito), build nativo para
Windows x64, melhorias de vídeo, e depois Steam Deck / Steam Machine (Linux).

Nome exibido: "Tyrian 2000+" (título da janela, About, READMEs, LEIA-ME). Em
nomes de arquivo, repositório, pastas e links continua tyrian2000plus /
Tyrian2000Plus, porque o "+" não serve neles.

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
  `dist/Tyrian2000Plus-windows-x64.zip` (Tyrian2000Plus.exe + SDL2.dll e SDL3.dll do sdl2-compat +
  SDL2_net.dll + LEIA-ME.txt (= README.pt-BR.md) e README.txt (= README.md) + licenças).
  Instalador: `./make_installer.sh <pasta-do-pacote> <saida.exe>` (NSIS,
  `windows/installer.nsi`): por usuário, sem admin, em
  %LOCALAPPDATA%\Programs\Tyrian2000Plus; o desinstalador apaga só os arquivos
  que instalou (lista gerada pelo script). A release anexa
  `Tyrian2000Plus-vX-windows-x64-setup.exe` (com dados) e
  `Tyrian2000Plus-vX-instalar-steam-deck.sh` (= linux/atualizar-tyrian.sh). O Makefile embute ícone e manifesto
  UTF-8 de `visualc/resources.rc`, e também nome e versão do exe (Propriedades >
  Detalhes, tirados do `git describe`). Os dois pacotes levam `CODIGO-FONTE.txt`
  (`write_source_note.sh`) com o link do código exato daquela versão (GPL).
- README principal em inglês (`README.md`); português em `README.pt-BR.md`.
  Cada mudança que chega ao jogador entra no `CHANGELOG.md` (em inglês), na
  seção "Next version" até virar release. Arquivos escritos neste projeto
  levam "Copyright (C) 2026 Tyrian 2000+ contributors"; os herdados mantêm o
  aviso do OpenTyrian.
- Linux x86_64 / Steam Deck: `./make_linux.sh` compila SDL2 estático (só
  depende da glibc) e gera `dist/Tyrian2000Plus-linux-x86_64.tar.gz`.
- GitHub Actions (`.github/workflows/build.yml`) roda os dois scripts e os
  testes a cada push; os pacotes ficam em "Artifacts" da execução e são
  anexados a uma release publicada. Só os pacotes da release levam os dados
  do jogo (originais, sem alteração, baixados por `add_game_data.sh`, com
  LEIA-ME-DADOS.txt de licença/créditos); os builds de cada push não levam.
  Segurança: todo download (llvm-mingw, SDL, sdl2-compat, dados, butler) é
  conferido por sha256 fixo nos scripts (trocou a versão, troque o sha256);
  o workflow só lê o repositório, e apenas o job "release" (que não roda nada
  baixado) escreve, anexando os pacotes e um SHA256SUMS. Actions fixadas por
  commit. O butler tem versão fixa em `itch_push.sh`.
- Steam Deck: `linux/atualizar-tyrian.sh` instala/atualiza pela última
  Release (sem chave, repositório público), conferindo o SHA256SUMS; com uma
  chave do GitHub em ~/.config/tyrian2000plus/github-token pega o build mais
  novo do Actions no main (conferido pelo digest do artifact). Só apaga a
  chave se o GitHub responder 401. O script fica todo entre { } porque a
  atualização grava a versão nova por cima dele enquanto ele roda. Também põe o jogo na Steam como jogo
  não-Steam (edita userdata/*/config/shortcuts.vdf, binário; guarda cópia
  .antes-tyrian), só no Modo Desktop e fechando a Steam com permissão;
  atalho que já mencione "tyrian" conta como existente.
- Os dados do jogo (freeware) ficam em `data/` ao lado do executável e não
  vão para o repositório.
- Testes: `tests/run_tests.sh [pasta-do-jogo]` compila e roda
  `tests/test_gamepad.c` (controles virtuais: genérico, Xbox, PS5 e Switch
  Pro, com tipo, nomes dos botões e ação de cada botão padrão), `tests/test_migrate.c`
  (cópia dos saves da pasta do OpenTyrian2000, inclusive quando falha) e, se receber uma pasta com
  data/, tyrian.sav e tyrian.cfg reais, `tests/test_saves.c` (carrega e
  regrava os saves; devem sair idênticos).
- Pastas do usuário: portátil (ao lado do exe) se existir `opentyrian.cfg` lá;
  senão `%APPDATA%/Tyrian2000Plus` ou `~/.config/tyrian2000plus` (desde o PR
  #24; antes era a pasta do OpenTyrian2000). `migrateUserDir()` em
  `src/file.c` COPIA os saves da pasta antiga na primeira vez (só se a nova
  não tiver tyrian.sav/tyrian.cfg/opentyrian.cfg) e não mexe na antiga; se a
  cópia falhar, usa a pasta antiga naquela vez e tenta de novo na próxima. No
  Linux a pasta nova é a mesma do github-token do updater.

## Controles

`src/joystick.c` usa SDL_GameController quando o SDL reconhece o dispositivo
(PAD_BUTTON/PAD_AXIS, seção "gamepad" no opentyrian.cfg) e cai para a API
antiga de joystick (AXIS/BUTTON/HAT) caso contrário. Hotplug é verificado a
cada 500 ms, inclusive durante as fases. Cada controle fica na sua posição
(vaga) do array `joystick[]` enquanto estiver conectado; o que sai deixa a
vaga livre ("Disconnected") e o próximo conectado ocupa a primeira vaga livre,
para cada jogador manter o seu controle. O array pode ser realocado em
`poll_joysticks()`/`check_joystick_hotplug()`: não guarde ponteiros para ele
através dessas chamadas.

Steam Controller (testado com o modelo 2026 via puck): o SDL2 não conhece
esse controle; o SDL3 (3.4+) tem driver próprio (`SDL_hidapi_steam_triton.c`).
Por isso o pacote Windows leva o sdl2-compat (SDL2.dll por cima do SDL3.dll,
ver `make_windows.sh`); o código continua compilando contra o SDL2. Sem a
Steam, o controle funciona e vibra (testado). Com a Steam aberta e o jogo
fora dela, a Steam também gera teclado/mouse do controle (botões contam duas
vezes, X abre o teclado virtual dela). Um filtro e outros remendos para isso
foram tentados (PRs #9, #10, #17) e REMOVIDOS a pedido do Gabriel (2026-10-19):
o jogo não sabe com certeza o que a Steam está fazendo. Com a Steam no modo
gamepad (segurar Start) tudo funciona (testado pelo Gabriel); a orientação é
essa, ou fechar a Steam ou abrir o jogo por ela; não tentar de novo sem ele pedir. O Linux
continua com SDL2 estático (lá o controle 2026 precisa da Steam). Tudo isso
está explicado no `README.pt-BR.md` (que vai no pacote como LEIA-ME.txt).

Ações do controle: 4 direções + 7 ações (`JOYSTICK_ASSIGNMENTS` em
`src/joystick.h`); a 7ª, "Both Sidekicks" (`both sidekicks` no cfg, padrão B),
dispara os dois sidekicks. Ação que falta num cfg antigo recebe o botão padrão
se ele estiver livre (`load_joystick_assignments`). No menu Joystick da nave a
ordem das linhas vem de `joystick_menu_assignment` (`src/game_menu.c`).

Tiro automático: opção "Auto Fire" em Setup > Controls (seção "gameplay",
`auto_fire` no opentyrian.cfg). Cada toque no tiro liga/desliga o disparo
contínuo, por jogador (`src/mainint.c`). O submenu Controls é onde entram as
próximas opções de controle.

Setup > Controls também tem Keyboard..., Controller... (com Buttons...) e
Mouse..., os mesmos ajustes do menu da nave, feitos no estilo do `setupMenu`
(`src/opentyr.c`). A captura de botão do controle fica em
`assign_joystick_action()` (`src/joystick.c`), usada pelos dois menus.

Controller Map: Setup > Controls > "Controller Map..." mostra um controle
genérico desenhado pixel a pixel (`src/padmap.c`, sem gráficos do jogo nem
formato de marca; o corpo é o mesmo para todos e só o layout muda: Xbox com o
analógico esquerdo acima do D-pad (também Nintendo), PlayStation com
touchpad, Steam Deck e sem controle no genérico; decisão do Gabriel em
2026-10-20), com a função de cada botão segundo o mapeamento atual
(inclui "Auto Fire"). Os botões acendem quando apertados; sai segurando B por 1 s (assim B e View também podem ser testados), com Esc ou com clique. Sem controle,
mostra o layout padrão (`joystick_default_gamepad_assignments()`). Com controle Xbox ou PlayStation, os botões redondos, Back/Start e os gatilhos do desenho levam o ícone do botão (`controlGlyph()`); os bumpers não, porque quase não aparecem acima do corpo.

Menu da nave: os itens Joystick, Keyboard e Mouse de Options (e Joystick e
Keyboard do menu de arcade) abrem as telas do Setup com
`setupMenuPage(SETUP_PAGE_*)` (`src/opentyr.c`), saindo direto de volta à
nave; as telas antigas (MENU_JOYSTICK_CONFIG etc. em `src/game_menu.c`)
ficaram no código, sem acesso, para não atrapalhar merges do upstream.
Setup > Controls > Shortcuts lista Alt+Enter, Esc, P, F1 e F11 (a fonte grande
não tem "+", por isso a tela mostra "Alt-Enter"). Setup tem "Game Speed" e
Graphics tem "Detail Level" (mesmos `gameSpeed`/`processorType` da pausa).
Options tem "Setup..." (antes do Done), que abre o mesmo
`setupMenu()` da tela de título e volta com `returnToItemScreen()`
(`src/game_menu.c`). O texto do item é inserido em `menuInt[3]` ao carregar
o `tyrian.hdt` (`src/helptext.c`); o menu de arcade (MENU_LIMITED_OPTIONS)
não tem o item.

Pausa durante a fase (Start/Esc, `JE_inGameSetup()` em `src/mainint.c`):
além dos itens do jogo, tem "Auto Fire", "Controls..." (abre o mesmo Setup >
Controls com `controlsMenu()` de `src/opentyr.c`; guarda a paleta e o fundo da
fase e volta com fade) e "Graphics..." (Display, Scaler,
Scaling Mode, Sharp Bilinear, VSync, Brightness), uma segunda página na mesma
caixa da pausa, desenhada sobre a fase com a paleta dela; mudanças valem na
hora. Controls e Graphics ficam escondidos em jogo em rede.

Controle desconectado no meio da fase: `check_joystick_hotplug()` marca a
perda e `joystick_take_lost()` faz o jogo pausar (`src/mainint.c`).

About: item "About" na tela de título, antes do Quit (inserido em
`menuText[6]` ao carregar o `tyrian.hdt`, `src/helptext.c`), e também em
Setup > "About..." (`aboutScreen()` em `src/about.c`) mostra o que é o
jogo, o recado do Gabriel, os créditos (os do jogo original são lidos do
`tyrian.cdt`) e links que abrem no navegador com `SDL_OpenURL`. O mesmo texto,
em português, está no `README.pt-BR.md` (e em inglês no `README.md`, o principal no GitHub); mudou um, mude os outros. O README original
do OpenTyrian2000 ficou em `doc/README-OpenTyrian2000`.

Aviso de atualização: ao abrir, `update_check_start()` (`src/update_check.c`)
consulta em segundo plano a última Release do GitHub (API pública, sem token;
WinHTTP no Windows, `curl` no Linux) e compara com `opentyrian_version` (o
`git describe`, ex. v1.0.2). Se houver mais nova, a tela de título mostra
"vX is out! See About." no canto e o About ganha os links de download (itch.io e GitHub). Opção
Setup > "Check Updates" (`check` na seção "updates", ligada por padrão).
Build sem tag de versão não consulta. Para testar o aviso, compile com
`VCS_IDREV='echo v1.0.1'`.

Log: o jogo grava `opentyrian.log` na pasta do usuário; peça esse arquivo
ao investigar problemas no PC do Gabriel.

Telas de ajuda: `padHelpText()` em `src/helptext.c` troca, só quando há um
controle conectado, as menções a teclas dos textos do `tyrian.hdt` (Enter, ESC,
setas, Alt-X, "[/]") pelos nomes dos botões do controle (conforme o mapeamento
atual). Texto que não reconhece fica igual.

Ícones dos botões: `src/padglyph.c` desenha em código (pixel art própria, sem
logos) ícones de botão para as famílias Xbox, PlayStation, Nintendo e Steam
Deck (`Pad_family` em `joystick.h`; o Deck é reconhecido pelo nome ou por
SteamDeck=1 com controle da Valve) e os acrescenta às
três tabelas de fonte (sprites 128+, códigos de caractere 0xB0+, ver
`padglyph.h`), então qualquer rotina de texto os desenha como letras, na cor
do texto. `joystick_button_icon()`, `joystick_action_icons()` e
`joystick_assignments_to_icons()` (`src/joystick.c`) dão o texto com ícones;
as versões `*_label`/`*_to_string` continuam em texto (testes, logs). Família
sem ícone para um botão cai no nome em texto.

## Tradução

Idiomas: Setup > "Language" (`lang_setting`; seção "language", `language` =
auto/en/pt-br no opentyrian.cfg; auto segue `SDL_GetPreferredLocales`).
Os dados do jogo não mudam: `src/lang.c` procura cada texto pelo original em
inglês (`tr()`, espaços do fim ignorados) numa tabela UTF-8 por idioma
(`src/lang_pt_br.c`), convertida para o CP437 do jogo. Os textos do
`tyrian.hdt` são traduzidos ao carregar (`lang_translate_reads` em
`readEncryptedString`), os nomes dos itens no fim de `JE_loadItemDat`, e os
textos do próprio programa com `tr()` na hora de desenhar. Trocar o idioma
recarrega hdt e itens (`lang_reload_texts`). Cada grupo da tabela diz o
tamanho máximo do buffer; a fonte grande (títulos) não tem números nem "-".
Não traduzir: códigos secretos (specialName), nomes padrão dos recordes,
listas antigas do DOS. As frases das ajudas que `padHelpText()` troca pelos
botões estão no fim da tabela ("Controller help"): se mudar o texto de uma
ajuda em PT, mantenha a frase exata. Acentos: `src/accents.c` monta as letras
acentuadas nas 3 fontes ao carregar (sprites 176+, códigos 0xF0+ para ã, õ e
maiúsculas que o CP437 não tem); nas duas fontes maiores as maiúsculas
ficam sem acento (não há espaço acima). Etapa 2 (datacubes e dicas das
fases, ~25 mil palavras) ainda não feita; antes, pedir permissão ao Jason
Emery. Estudo: `/mnt/project-files/analises/traducao-pt-br.md`.

## Vídeo

Padrão: Scaling Mode "Fit 4:3" (fiel ao monitor 4:3 de 1995). A opção
"Sharp Bilinear" em Setup > Graphics (`sharp_bilinear` na seção "video",
desligada por padrão) amplia primeiro por um número inteiro (pixels nítidos)
e depois encaixa na janela com filtro bilinear, para os pixels ficarem do
mesmo tamanho quando a ampliação não é inteira (`sharp_bilinear_prescale()`
em `src/video.c`). Em múltiplos exatos nada muda.

Brilho: Setup > Graphics > Brightness é o mesmo `gammaCorrection` da tecla
F11 (salvo no tyrian.cfg); só vale dentro das fases.

Análise de usabilidade dos menus (o que falta, como "vídeo na pausa"):
`/mnt/project-files/analises/usabilidade-menus.md` no projeto.

Steam Deck: o modo "Integer" já dá o encaixe exato 4x (1280x800, pixels
quadrados), então não há opção nova para isso. O jogo não muda a taxa da tela
no Modo de Jogo (o gamescope/Steam controla); o LEIA-ME explica 70 Hz no OLED
e 60 Hz no LCD (40-60 Hz, nada combina com 35 FPS).
