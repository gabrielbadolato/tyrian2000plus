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
- Versão: o número é só a tag (o programa lê com `git describe`). Release
  pelo botão Actions > "Prepare release" (`.github/workflows/prepare-release.yml`
  + `prepare_release.sh`): escolhe novidade (v1.0.5 -> v1.1), correcao
  (-> v1.1.1), grande (-> v2.0) ou versão exata; abre o PR `release/vX` que
  troca "## Next version" do CHANGELOG por "## vX (data)" e dispara o Build
  nele (push feito pelo GITHUB_TOKEN não dispara workflows sozinho). Ao
  mesclar, o job "draft" cria um RASCUNHO de release com a seção do CHANGELOG
  como notas (EN; o PT é colado à mão). O Gabriel publica o rascunho; daí
  segue o build.yml de sempre. Precisa da opção "Allow GitHub Actions to
  create and approve pull requests"; sem ela o resumo da execução dá o link
  para abrir o PR à mão.
- Navegador (itch.io): `./make_web.sh [--with-data]` compila o mesmo código
  com Emscripten (SDL2 port do mesmo fonte do make_linux.sh, ASYNCIFY: cada
  SDL_Delay devolve o controle ao navegador, então os laços do jogo ficaram
  como estão) e gera `dist/Tyrian2000Plus-web/` (index.html de `web/`,
  tyrian.js/.wasm e `data/tyrian2000.tar` com os dados sem alteração, só com
  --with-data). A página guarda a pasta do usuário no IndexedDB, pede um clique
  antes de começar (som) e tem botão de tela cheia (trava o Esc no Chrome).
  Diferenças no C ficam em `#ifdef __EMSCRIPTEN__`: sem rede e sem aviso de
  atualização (o item some do Setup), a janela segue o tamanho da página
  (`window_fit_page()` em `src/video.c`) e cada quadro devolve a vez ao
  navegador. Job "Web (browser)" no build.yml; na release vai ao itch.io pelo
  canal "html5" (na página do itch: marcar o arquivo como "played in the
  browser"). Teste local: `python3 -m http.server -d dist/Tyrian2000Plus-web`.
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
- Robustez: `tests/run_autoplay.sh <pasta-data>` (uns 90 min; só local, os
  dados não vão ao CI) compila com AddressSanitizer/UBSan e roda
  `tests/test_autoplay.c` (o modo "constant play" do jogo, -c, joga cada
  episódio sozinho, invencível, nas dificuldades do menu e por dois caminhos
  do mapa; relógio virtual, então uma fase leva segundos; um piloto mira no
  inimigo mais baixo e, se a fase parar 1 min, tira da tela os que sobraram
  como se tivessem fugido, com o pulo 254 e as flags que a fase espera) e
  `tests/test_monkey.c` (teclas aleatórias desde a tela de título por 30 min
  de jogo; Quit volta ao título). Rodar depois de mudanças grandes ou antes de
  release grande. A primeira rodada (2026-10-20) achou a leitura além de
  `mainMenuHelp` em `JE_drawMainMenuHelpText` (herdada do upstream).
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

Vibração: Setup > Controls > "Vibration" (`rumble_strength` em
`src/joystick.c`, `vibration` = off/weak/strong na seção "gameplay"; Weak é
40% da força). Bateria: `check_joystick_batteries()` (no hotplug) avisa uma
vez por controle quando fica fraca; a fase mostra o aviso
(`joystick_take_low_battery()`) e o Controller Map mostra o nível.

Teclado na tela (`src/osk.c`): ao digitar o nome do save (`JE_operation`) e
dos recordes (`JE_highScoreCheck`, em `src/mainint.c`), com controle
conectado aparece um teclado embaixo da caixa do nome. Só reage aos toques
do controle (as teclas que `push_joysticks_as_keyboard()` gera têm `sym` 0);
Start (segurado na hora do Enter) termina e Back (no Esc) cancela.

Comparar na loja: no menu Upgrade Ship da nave, cada item mostra ao lado do
preço um número (`src/shopstats.c`, desenhado em `JE_itemScreen`): dano por
segundo estimado para armas (na potência da linha, `temp_weapon_power`) e
sidekicks, casco (`ships[].dmg`), escudo (`mpwr`) e energia do gerador; verde
se for melhor que o item comprado (`old_items`), vermelho se pior. O dano só
conta acertos diretos (tiro que atravessa conta ~20 px de contato; reação em
cadeia soma um disparo da arma da explosão); alguns níveis do jogo trocam dano
por espalhamento/teleguiado, então o número pode cair ao subir de nível.

Jukebox (`src/jukebox.c`): com controle, X para/toca, Y esconde o texto e
LB/RB tocam os efeitos sonoros (as letras do teclado continuam valendo); a
tela mostra os botões no lugar das teclas.

Simular o Steam Deck: `tests/run_deck_sim.sh <pasta-data> <roteiro> <pasta-prints>`
roda o jogo numa tela virtual 1280x800 (Xvfb + matchbox-window-manager,
que deixa a janela cheia como o Modo de Jogo), com SteamDeck=1 e o controle
virtual da Steam (28de:11ff), apertando botões e tirando prints conforme um
roteiro (`tests/deck/*.txt`, formato em `tests/deck_sim.c`). Sem pasta de
usuário, começa sem configuração (primeira vez no Deck). Use em vez de pedir
ao Gabriel para testar no Deck.

Continuar: item "Continue" na tela de título (`menuText[0]`, escondido sem
jogo salvo), carrega `lastSaveSlot` (último slot salvo ou carregado,
`last_save` na seção "gameplay"; sem ele, o "LAST LEVEL" do slot 11).

Salvamento automático: o slot "LAST LEVEL" (11, ou 22 em dois jogadores; o
jogador não salva nele) é gravado também ao chegar na nave (evento 'I' em
`JE_loadMap`, `src/tyrian2.c`), não só ao começar a fase; não grava logo
depois de carregar um jogo (`gameJustLoaded`). `JE_saveGame` grava o
opentyrian.cfg quando muda `lastSaveSlot`, para o Continue sobreviver a um
fechamento à força. Retomar no meio da fase (via demo) é o próximo passo.

Retomar fase e Replay (`src/levelrec.c`): toda fase é gravada enquanto se
joga em `lastlevel.rec` (pasta do usuário): cabeçalho com o save "LAST LEVEL"
do início da fase, os dois `Player` e as globais que passam de uma fase para
outra (`StartState`), a semente do `mt_rand`, e depois, quadro a quadro, a
entrada de cada jogador já combinada (o ponto onde o modo em rede envia a dele,
em `JE_playerMovement`) e, a cada 35 quadros, um checksum do estado. Se o jogo
fecha no meio da fase (sem o TOKEN_END), "Continue" (só se o save do slot
11/22 ainda é o do início da fase) reproduz escondido e rápido até ali
(`levelrec_fast()`, sem som nem espera; mostra "Resuming the level... N%") e
pausa. Sair da fase pelo menu (Quit) ou terminá-la grava o fim: não retoma.
Teclas de trapaça/debug no meio da fase param a gravação ali. "Replay" na tela
de título (`MENU_ITEM_REPLAY`, escondido sem gravação) mostra a gravação
(`levelrec_watching()`), sem salvar nada nem contar estatísticas. Se um
checksum não bate, a retomada desiste e a fase recomeça (volta à nave, como
ao morrer). Teste: `tests/run_resume.sh <pasta-data>` (ASan/UBSan; com
TEST_MANUAL=1 usa tiro, sidekicks, Auto Fire e armas especiais aleatórias;
5º argumento 2 = dois jogadores) grava cada fase de cada episódio, retoma,
retoma de novo e assiste, conferindo quadro a quadro. Global nova que passe de
uma fase para a outra e mude a fase precisa entrar no `StartState` (o teste
acha: foi assim com `portConfigDone` e a dificuldade do episódio 5).

Janela: posição, tamanho e maximizado ficam em `window_*` na seção "video"
(`window_geometry`, `video_remember_window()` em `src/video.c`); volta ao
abrir e ao sair da tela cheia; se o lugar não está em nenhuma tela, centraliza.
Fechar a janela (SDL_QUIT) chama `JE_tyrianHalt(0)`, que salva as configurações.

Estatísticas: Setup > "Statistics..." (`statisticsScreen()` em
`src/stats.c`) mostra os totais de todos os jogos (tempo nas fases, fases
concluídas, inimigos, mortes, fases secretas, datacubes, maior pontuação) e
os recordes de cada fase (episódio + nome: pontos ganhos na fase, menor tempo,
maior % destruída). Ficam em `statistics.cfg` na pasta do usuário (formato do
config_file), separado do tyrian.sav. `JE_main` chama
`stats_level_start/frame/end` (`src/tyrian2.c`); o tempo conta quadros do
laço da fase (35 por segundo), então pausa e velocidade do jogo não mudam os
recordes. Demos, `-c`/`-k` e a invencibilidade (tecla C) não contam. Quando
bate um recorde que já existia, a tela de fim de fase original
(`JE_endLevelAni`) ganha a linha "New level record!". A tela de resultado
nova (B3) foi descartada: o jogo já mostra % destruídos e datacubes ali.

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

Novidades (What's New): Setup > "What's New..." e o primeiro link do About
abrem `whatsNewScreen()` (`src/about.c`, mesma tela rolável do About, título
"Version History" porque a fonte grande não tem apóstrofo). O texto vem de
`whatsnew_build()` (`src/whatsnew.c`): as notas das Releases do GitHub (a
mesma consulta do aviso de atualização, agora `/releases?per_page=10`,
`update_check_releases()`), guardadas em `release-notes.txt` na pasta do
usuário para uso offline, e o `CHANGELOG.md` embutido no programa pelo
Makefile (`obj/changelog.inc`, um texto por linha; `tests/run_tests.sh` gera
o mesmo). Das notas da Release mostra a parte "**English**" ou
"**Português**" conforme o idioma (o que vem antes de qualquer título conta
como inglês): AO COLAR O PT NAS NOTAS DE UMA RELEASE, pôr o título
"**Português**" antes dele. Versão mais nova que a instalada aparece primeiro
("vX - new"), então até uma versão antiga mostra o que a nova traz. A fonte
pequena não tem ">" (vira "/"). Depois de atualizar, a tela de título mostra
"New in vX! See About." até abrir a tela (`notes_seen` na seção "updates";
sem opentyrian.cfg conta como já visto). Teste: `tests/test_whatsnew.c`
(aceita um JSON salvo da API como argumento).

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

Tela de toque (`src/touch.c`, 2026-10): direcional flutuante à esquerda e
botões TIRO, SIDEKICKS, MODO e MENU à direita, nas faixas ao lado do jogo
(em pé, o jogo sobe e os controles ficam embaixo, `touch_adjust_game_rect`);
desenhados com o renderer do SDL em `present()` (`src/video.c`), com a fonte
do jogo. Aparecem no primeiro SDL_FINGERDOWN e somem com tecla real (sym != 0)
ou controle (`poll_joystick`). O toque-vira-mouse do SDL fica desligado; dedo
sobre o jogo vira mouse (SDL_TOUCH_MOUSEID, sem ponteiro): toque = clique nos
menus, arrastar move a nave. Na fase entra em `JE_playerMovement` como
analógico (antes da gravação do levelrec); nos menus vira setas/Enter/Esc em
`push_joysticks_as_keyboard`. Mesmo código serve para um Android no futuro.
Teste: `tests/test_touch.c`.

Jogos salvos entre PC e navegador: Setup > "Saved Games..." abre a pasta do
usuário no gerenciador de arquivos (`userDirOpenInFileManager()`,
`src/file.c`); no navegador abre a janela da página (`window.tyrianSaves`,
`web/index.html`) que baixa/carrega o tyrian.sav (guarda o anterior como
tyrian-antes-de-carregar.sav / tyrian-before-loading.sav e recarrega a
página). O tyrian.sav é o mesmo em todas as versões. `config.c` lembra o
hash do arquivo: se outro tyrian.sav for posto na pasta com o jogo aberto,
a tela de título relê (`reloadSavesIfReplaced`) e o fechamento não grava por
cima (`saveSavesUnlessReplaced`). Teste: `tests/test_savefile.c`.

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

Música: Setup > Sound > "Music Sound" (`src/oplchip.c`, `music_chip` =
accurate/classic na seção "sound"). Classic é o padrão (pedido do Gabriel em
2026-10-21); o cfg só vale se `music_chip_chosen = yes` (gravado quando o
jogador escolhe no menu), porque a v1.1/v1.2 sempre gravavam "accurate".
Accurate usa o Nuked OPL3 1.8
(`src/opl3.c`, LGPL 2.1+, sem alteração); Classic é o emulador do DOSBox
(`src/opl.c`). Os dois recebem todas as escritas; o volume do Nuked foi
igualado medindo as 41 músicas (fator 6/5).

Scaler: o padrão é "None" desde a v1.4 (antes Scale2x; pedido do Gabriel em
2026-10-21). Como o cfg sempre grava o scaler, "Scale2x" salvo só vale com
`scaler_chosen = yes` (gravado quando o jogador escolhe no menu); outro scaler
salvo é escolha e fica. A janela nunca abre menor que 640x400
(`window_set_scaler_size()` em `src/video.c`), porque o None é só 320x200.

Brilho: Setup > Graphics > Brightness é o mesmo `gammaCorrection` da tecla
F11 (salvo no tyrian.cfg); só vale dentro das fases.

Análise de usabilidade dos menus (o que falta, como "vídeo na pausa"):
`/mnt/project-files/analises/usabilidade-menus.md` no projeto.

Primeira vez no Steam Deck (sem opentyrian.cfg; SteamDeck=1 ou placa "Valve"
em /sys/devices/virtual/dmi/id/board_vendor, `running_on_steam_deck()` em
`src/config.c`): tela cheia + Sharp Bilinear + Smooth Motion, mantendo Fit 4:3
(decisão do Gabriel em 2026-10-21). Quem já tem cfg não muda.

Movimento suave (`src/smooth.c`, Setup > Graphics > "Smooth Motion" e na
página Graphics da pausa; `smooth_motion` na seção "video", desligado por
padrão, ligado na 1ª vez no Deck; escrito do zero, SEM código do Modern Tyrian
nem do Engaged, a pedido do Gabriel): a fase continua a 35 quadros/s. Cada
primitiva de desenho (blit_sprite*, JE_pix, fill_rectangle_xy, JE_barShade,
fundos `draw_background_layer`, filtros lava/água/blur, estrelas,
superpixels) chama `SMOOTH_RECORD` e entra numa lista do quadro, quando o
destino é game_screen ou VGAScreen2 (a HUD em VGAScreenSeg não entra).
`smooth_tag()` diz de quem é o desenho (inimigo i, tiro z, explosão, nave,
sidekick, estrela, fundo). Em `JE_starShowVGA`, `smooth_frame_due()` redesenha
a lista sobre a cópia do quadro anterior e confere que sai igual (memcmp; se
não sai, o quadro aparece como antes), e até o quadro vencer mostra imagens
com cada coisa marcada no meio do caminho entre o quadro anterior e este
(salto > 40 px não desliza; fundo pela linha/coluna do mapa), uma por
atualização do monitor, nunca mais rápido que ele, nem sem VSync. O quadro em
si não é mostrado (a próxima imagem parte dele), então a simulação começa na
hora certa. Nada disso lê ou muda o estado do jogo. Primitiva nova que desenhe
na fase precisa do `SMOOTH_RECORD` (senão o quadro sai diferente e perde o
efeito); objeto novo que se mova precisa de `smooth_tag`. Testes:
`test_autoplay` e `test_resume` ligam `smooth_test_enable` (conta quadros
iguais/diferentes; tem de dar 0 diferentes) e `SMOOTH_DUMP=<pasta>:<quadro>:<n>`
salva BMPs das imagens intermediárias. `TYRIAN_SMOOTH_STATS=1` escreve no log
quantas imagens por segundo; `TYRIAN_SMOOTH_HZ` força a taxa do monitor.
Imagens finas (v1.5): nas fases cada imagem do meio sai até 4x maior
(`video_fine_scale()` = quanto cabe inteiro em 320x200 no retângulo da
janela, 1..4, só com scaler None; `smooth_fine` na seção "video", sem item no
menu: liga junto do Smooth Motion, decisão do Gabriel). Escala s: s×s fases,
cada uma um redesenho 1x da lista com as primitivas de sempre; na fase (i,j)
uma posição fina P = de*s + round((para-de)*s*alpha) vira
`floorDiv(P - i + s - 1, s)` (`phased()`), e `drawFine()` entrelaça as
fases (fine[(y*s+j)*W + x*s+i]). `video_present_fine()` mostra numa textura
própria (Sharp Bilinear junto). Se desenhar levar mais que meia atualização
do monitor, ou desenhar e mostrar mais que 1,25 atualização (média de 16;
VSync espera no máximo uma), cai para s-1 e o log avisa. Imagem que demorou
mais que isso (`slowPicture`, PC sem placa de vídeo, navegador com GL por
software) faz as próximas começarem cedo o bastante para não atrasar o
quadro; vai pela metade a cada quadro sem imagem. O teste confere que em
alpha=65536 toda fase é o próprio quadro. `SMOOTH_DUMP_FINE=<s>` com
SMOOTH_DUMP salva também as imagens finas. Menus/fades seguem em 1x.
Fora das fases: `smooth_show_until_due(frames, show, data)` faz o mesmo
ritmo (uma imagem por atualização até o quadro vencer) chamando `show(alpha)`;
usado nos fades (`fade_steps()` em `src/palette.c`, paleta no meio do caminho
entre um passo e outro, o passo final é mostrado no fim) e no logo da tela de
título (`show_logo_picture` em `src/tyrian2.c`). A prévia de armas do menu da
nave (`JE_weaponViewFrame`) grava estrelas e tiros na tela (`smooth_sim_begin`
/`smooth_sim_drawn`, mesma conferência) e `smooth_sim_show(3)` troca o
JE_showVGA: dentro do retângulo 8,8-143,182, o pixel que nada desenhou por
cima depois (moldura, nave, texto) vem da imagem intermediária, e o ponteiro
do mouse é tirado (`mouse_pointer_remove`) e redesenhado onde o mouse está.
Ponteiro nos menus: `JE_mouseReplace` marca que a tela foi mostrada com ele
(`vga_shows` conta os JE_showVGA) e `waitUntilElapsed` chama
`mouse_pointer_between()`, que mostra a tela (já sem ponteiro) com o ponteiro
na posição atual quando o mouse se move, no ritmo de `smooth_picture_wait()`.

Steam Deck: o modo "Integer" já dá o encaixe exato 4x (1280x800, pixels
quadrados), então não há opção nova para isso. O jogo não muda a taxa da tela
no Modo de Jogo (o gamescope/Steam controla); o LEIA-ME explica 70 Hz no OLED
e 60 Hz no LCD (40-60 Hz, nada combina com 35 FPS).
