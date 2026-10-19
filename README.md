# Tyrian2000 Plus

Tyrian 2000 com controles modernos, para Windows, Linux e Steam Deck.

**Projeto de fã, gratuito e não oficial.** Não é feito, vendido nem apoiado
pela Eclipse Software, por Jason Emery ou pelas editoras originais. Os dados
do jogo são os da versão freeware original, sem nenhuma alteração.

## O que é

Tyrian é um jogo de nave com rolagem vertical, lançado em 1995. A história se
passa no ano 20.031: você é Trent Hawkins, um piloto de caça que precisa salvar
a galáxia. O jogo foi criado pela Eclipse Software e publicado pela Epic
MegaGames. O Tyrian 2000 (1999) trouxe um quinto episódio. Depois, Jason Emery
liberou o jogo como freeware.

O Tyrian2000 Plus é baseado no [OpenTyrian2000](https://github.com/KScl/opentyrian2000),
que por sua vez vem do [OpenTyrian](https://github.com/opentyrian/opentyrian), a
versão de código aberto do Tyrian. O que esta versão acrescenta:

- Suporte a controles modernos: Xbox, PlayStation, Switch Pro, Steam Controller
  e outros, com vibração e troca de controle sem reiniciar.
- Desenho do controle mostrando a função de cada botão (Controller Map).
- Tiro automático (Auto Fire).
- Nomes dos botões do controle nas telas de ajuda.
- Mais opções de vídeo, inclusive na pausa, e o filtro Sharp Bilinear.
- Versões prontas para Windows, Linux e Steam Deck.
- Tela About com a história do projeto e os créditos.

## Um recado do Gabriel

Oi! Eu sou o Gabriel Badolato. Não sou desenvolvedor de jogos e sou iniciante
em programação. O Tyrian marcou a minha infância, e fiz este projeto para poder
jogá-lo melhor hoje, com controles e telas modernos. Fiz tudo com muita ajuda
do Claude, um assistente de IA da Anthropic. Obrigado por jogar!

## Instalação

Os pacotes não trazem os dados do jogo. Baixe-os à parte (são gratuitos):
<https://www.camanis.net/tyrian/tyrian2000.zip>

### Windows

1. Baixe o `Tyrian2000Plus-windows-x64.zip` e descompacte numa pasta sua.
2. Descompacte o `tyrian2000.zip` e copie **todos** os arquivos de dentro da
   pasta `tyrian2000` (tyrian.hdt, tyrian1.lvl, music.mus etc.) para a pasta
   `data`, ao lado do `Tyrian2000Plus.exe`.
3. Conecte o controle e abra o `Tyrian2000Plus.exe`.

### Steam Deck e Linux

1. No Modo Desktop, descompacte o `Tyrian2000Plus-linux-x86_64.tar.gz`
   (botão direito > Extrair aqui) numa pasta sua, por exemplo `/home/deck/Jogos`.
2. Copie os dados do jogo para a pasta `data`, como no passo 2 do Windows.
3. Na Steam: "Adicionar um jogo" > "Adicionar um jogo não-Steam" > "Procurar",
   escolha o arquivo `Tyrian2000Plus` e confirme.
4. Volte ao Modo de Jogo e abra o Tyrian2000Plus pela biblioteca.

O arquivo `Tyrian2000Plus.png` pode ser usado como ícone na Steam. O script
`atualizar-tyrian.sh` faz os passos 1 e 2 sozinho e cria um atalho no menu.

**Tela do Deck:** o padrão é "Fit 4:3", fiel aos monitores de 1995. Para tela
cheia com pixels nítidos, use Setup > Graphics > Scaling Mode > "Integer". O
jogo roda a 35 quadros por segundo: no Deck OLED, ponha a taxa de atualização
em 70 Hz (perfil por jogo); no Deck LCD, deixe em 60 Hz. Não ligue o limite de
quadros da Steam.

## Controles

| Controle (Xbox)       | Função                              |
|-----------------------|-------------------------------------|
| Analógico ou D-pad    | Mover a nave                        |
| A ou RT               | Atirar                              |
| Y                     | Trocar o modo da arma traseira      |
| LB ou LT              | Sidekick esquerdo                   |
| RB ou X               | Sidekick direito                    |
| Start (Menu)          | Menu do jogo                        |
| Back (View)           | Pausa                               |

Nos menus, A confirma e B volta. Nos controles PlayStation e Switch Pro, os
botões nas mesmas posições fazem a mesma coisa. Os botões podem ser trocados
em Setup > Controls > Controller... > Buttons.

Teclado: setas movem, Espaço atira, Enter troca a arma traseira, Ctrl e Alt
usam os sidekicks, Esc abre o menu, P pausa, Alt+Enter alterna a tela cheia,
F1 mostra a ajuda e F11 muda o brilho.

## Onde fica cada opção

- **Tela de título > Setup:** vídeo (Graphics), som (Sound), controles
  (Controls), Jukebox e About.
- **Setup > Controls:** teclado, controle, mouse, Auto Fire e o desenho do
  controle (Controller Map).
- **Menu da nave (entre as fases) > Options > Setup...:** o mesmo Setup.
- **Durante a fase, Start ou Esc:** volume, detalhes, velocidade, Auto Fire e
  Graphics... (tela cheia, escala, Sharp Bilinear, VSync e brilho, aplicados
  na hora).
- **Setup > About:** o que é o jogo, de onde veio, os créditos e os links
  (Enter ou A abre o link no navegador).

## Dicas do Steam Controller

- No Windows, o jogo lê o Steam Controller direto, sem precisar da Steam:
  botões, nomes dos botões e vibração funcionam.
- Com a Steam aberta, ela também transforma o controle em teclado e mouse. O
  jogo percebe isso e ignora esse teclado e mouse enquanto você usa o controle.
- Se algum botão contar duas vezes, ou o Y abrir o teclado virtual da Steam,
  **segure o botão Start** até aparecer o aviso da Steam: isso desliga o modo
  teclado e mouse dela.
- Outra opção é **adicionar o jogo na Steam** como jogo não-Steam e abri-lo
  pela biblioteca.
- No Linux e no Steam Deck, o Steam Controller 2026 só funciona com o jogo
  aberto pela Steam.

## Problemas

O jogo grava o arquivo `opentyrian.log` na pasta do jogo (ou em
`%APPDATA%\OpenTyrian2000` no Windows e `~/.config/opentyrian2000` no Linux).
Envie esse arquivo ao relatar um problema.

## Licença

- O programa é software livre, sob a GNU General Public License, versão 2 ou
  posterior (arquivo `COPYING`). O código-fonte está neste repositório.
- Os dados do Tyrian 2000 são freeware. A licença de Jason Emery diz: *"Feel
  free to play it all you want and share it with friends."* (jogue o quanto
  quiser e compartilhe com os amigos). Veja `doc/tyrian-freeware-license.txt`.
- Este projeto é e sempre será gratuito.

## Créditos

**Tyrian e Tyrian 2000** (créditos do próprio jogo): Eclipse Software.
Jason Emery (programação, design das fases e tudo mais), Alexander Brandon
(história e música), Andreas Molnar (sistema de som LOUDNESS), Daniel Cook
(arte do jogo e da interface), Cayenne Mandua (arte do Tyrian 2000), Devon
Tackett, John Calcagno, Andrew Paquette, Arturo Sinclair, Bruce Hsu, Markus
Mueller, Susan Aldworth, Nando Eweg, o produtor Robert Allen e os testadores
Aaron Hartman, Lee Cheramie e Andy Lehrfeld. A lista completa aparece em
Setup > About.

**OpenTyrian:** The OpenTyrian Development Team e colaboradores: Carl Reinke,
Yuri Kunde Schlesner, mvcixT, syntaxglitch, Alex Rodionov, Stephan T. Lavavej,
emacs.hacker, Miro Kropacek, Alexandre Detiste, sweetlilmre, Sander van Dijk,
a.h.vandijk, qon, Adam Vandenberg, Dmitry Marakasov, Jan Alexander Steffens,
Jeff Smith, Kane, Pedro Catalão, Renato Alves, Sergii Pylypenko, George
Rawlinson, mcpusc, norm2782, okkirikomi, Patryk Obara e AshTR.

**OpenTyrian2000:** Kaito Sinclaire (KScl).

**Tyrian2000 Plus:** Gabriel Badolato, com ajuda do Claude (Anthropic).

**Agradecimentos:** Sam Lantinga e a equipe do [SDL](https://www.libsdl.org/),
o [camanis.net](https://www.camanis.net/tyrian/) por manter os dados freeware
disponíveis, e os fãs do Tyrian que mantêm o jogo vivo.

## Links

- Tyrian2000 Plus: <https://github.com/gabrielbadolato/tyrian2000plus>
- OpenTyrian2000: <https://github.com/KScl/opentyrian2000>
- OpenTyrian: <https://github.com/opentyrian/opentyrian>
- Dados freeware do Tyrian 2000: <https://www.camanis.net/tyrian/>

## Para quem quer compilar

- Windows x64 (a partir do Linux): `./make_windows.sh`
- Linux x86_64 / Steam Deck: `./make_linux.sh`
- Testes: `tests/run_tests.sh [pasta-do-jogo]`

O texto original do OpenTyrian2000 (em inglês) está em
`doc/README-OpenTyrian2000`.
