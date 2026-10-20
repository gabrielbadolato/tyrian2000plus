# Tyrian 2000+

[Português](README.md) | **English**

Tyrian 2000 with modern controllers, for Windows, Linux and the Steam Deck.

**A free, unofficial fan project.** It is not made, sold or endorsed by
Eclipse Software, Jason Emery or the original publishers. The game data is the
original freeware release, unchanged.

## What it is

Tyrian is a vertical scrolling shooter released in 1995. It is set in the year
20,031: you are Trent Hawkins, a fighter pilot out to save the galaxy. The game
was made by Eclipse Software and published by Epic MegaGames. Tyrian 2000
(1999) added a fifth episode. Jason Emery later released the game as freeware.

Tyrian 2000+ is based on [OpenTyrian2000](https://github.com/KScl/opentyrian2000),
which in turn comes from [OpenTyrian](https://github.com/opentyrian/opentyrian),
the open-source port of Tyrian. What this version adds:

- Modern controller support: Xbox, PlayStation, Switch Pro, Steam Controller
  and others, with rumble and hot-plugging.
- A drawing of the controller showing what each button does (Controller Map).
- Auto Fire.
- Controller button names in the help screens.
- More graphics options, including in the pause menu, and a Sharp Bilinear
  filter.
- Ready-to-play builds for Windows, Linux and the Steam Deck.
- An About screen with the story of the project and the credits.

## A note from Gabriel

Hi! I'm Gabriel Badolato, from Brazil.

Tyrian 2000 was a big part of my childhood. It was one of the games that made
me love computers, and years later I worked with them for a living. I wanted
to play it again with a modern controller, on a big screen and on the Steam
Deck. So I started this project.

I'm not a game developer. I worked in IT for years, and now I'm finishing a
degree in biomedicine, hoping to work in bioinformatics. I learned to program
in college, but I never wrote much code. Claude, an AI assistant made by
Anthropic, wrote most of it. I tested every build on my own PC and
controllers.

Thank you to Jason Emery, Alexander Brandon, Daniel Cook and everyone at
Eclipse Software, and to the OpenTyrian and OpenTyrian2000 developers who kept
the game alive.

This project is free, and it always will be.

Thank you for playing!

## Installation

The packages of the published versions (GitHub Releases page and itch.io)
already include the original game data: just extract and play (skip step 2).
If the `data` folder is empty, download the data separately (it's free):
<https://www.camanis.net/tyrian/tyrian2000.zip>

### Windows

1. Download `Tyrian2000Plus-windows-x64.zip` and extract it to a folder.
2. Extract `tyrian2000.zip` and copy **all** the files inside its `tyrian2000`
   folder (tyrian.hdt, tyrian1.lvl, music.mus etc.) into the `data` folder,
   next to `Tyrian2000Plus.exe`.
3. Plug in your controller and run `Tyrian2000Plus.exe`.

### Steam Deck and Linux

1. In Desktop Mode, extract `Tyrian2000Plus-linux-x86_64.tar.gz` (right click >
   Extract here) to a folder, for example `/home/deck/Games`.
2. If the `data` folder is empty, copy the game data into it, as in step 2
   for Windows.
3. In Steam: "Add a Game" > "Add a Non-Steam Game" > "Browse", pick the
   `Tyrian2000Plus` file and confirm.
4. Go back to Game Mode and start Tyrian2000Plus from your library.

`Tyrian2000Plus.png` can be used as the icon in Steam. The
`atualizar-tyrian.sh` script, included in the GitHub package, does steps 1
and 2 for you, adds a menu shortcut and later updates the game. If you
installed through the itch.io app you don't need it: the app updates itself.

**Deck screen:** the default is "Fit 4:3", true to 1995 monitors. For full
screen with sharp pixels, use Setup > Graphics > Scaling Mode > "Integer". The
game runs at 35 frames per second: on the Deck OLED, set the refresh rate to
70 Hz (per-game profile); on the Deck LCD, leave it at 60 Hz. Don't turn on
Steam's frame limit.

## Controls

| Controller (Xbox)     | Action                              |
|-----------------------|-------------------------------------|
| Left stick or D-pad   | Move the ship                       |
| A or RT               | Fire                                |
| Y                     | Change the rear weapon mode         |
| LB or LT              | Left sidekick                       |
| RB or X               | Right sidekick                      |
| Start (Menu)          | Game menu                           |
| Back (View)           | Pause                               |

In menus, A confirms and B goes back. On PlayStation and Switch Pro
controllers, the buttons in the same positions do the same thing. Buttons can
be changed in Setup > Controls > Controller... > Buttons.

Keyboard: arrows move, Space fires, Enter changes the rear weapon, Ctrl and
Alt fire the sidekicks, Esc opens the menu, P pauses, Alt+Enter toggles full
screen, F1 shows help and F11 changes the brightness.

## Where each option is

- **Title screen > About:** what the game is, where it came from, the credits
  and the links (Enter or A opens a link in your browser).
- **Title screen > Setup:** Graphics, Sound, Controls, Jukebox, Check Updates
  and About.
- **Setup > Controls:** keyboard, controller, mouse, Auto Fire and the
  Controller Map.
- **Ship menu (between levels) > Options > Setup...:** the same Setup.
- **During a level, Start or Esc:** volume, detail level, game speed, Auto
  Fire and Graphics... (full screen, scaling, Sharp Bilinear, VSync and
  brightness, applied right away).
- **Setup > About:** the same About screen, to see it in the middle of a
  saved game.
- **Setup > Check Updates:** at startup the game asks GitHub for the latest
  published version. If there is a newer one, a note shows in the corner of
  the title screen, and About has the links to download it (itch.io and GitHub). None of your data
  is sent; without internet, nothing shows. Set it to "Off" to turn it off.

## Steam Controller tips

- On Windows, the game reads the Steam Controller directly, without Steam:
  buttons, button names and rumble all work.
- **With Steam open**, keep the controller in Steam's **gamepad mode**: hold
  the Start button until Steam shows its notice. Then everything works
  normally. In Steam's keyboard and mouse mode, buttons count twice and X
  opens Steam's on-screen keyboard; the game cannot prevent that.
- Closing Steam before playing also works, as does adding the game to Steam
  as a non-Steam game and starting it from your library.
- On Linux and the Steam Deck, the 2026 Steam Controller only works when the
  game is started from Steam.

## Problems

The game writes `opentyrian.log` in the game folder (or in
`%APPDATA%\OpenTyrian2000` on Windows and `~/.config/opentyrian2000` on Linux).
Please send that file when reporting a problem.

## License

- The program is free software under the GNU General Public License, version 2
  or later (see `COPYING`). The source code is in this repository.
- The Tyrian 2000 data is freeware. Jason Emery's license says: *"Feel free to
  play it all you want and share it with friends."* See
  `doc/tyrian-freeware-license.txt`.
- This project is free and always will be.

## Credits

**Tyrian and Tyrian 2000** (from the game's own credits): Eclipse Software.
Jason Emery (programming, level design and everything else), Alexander Brandon
(story and music), Andreas Molnar (LOUDNESS sound system), Daniel Cook (game
and interface artwork), Cayenne Mandua (Tyrian 2000 artwork), Devon Tackett,
John Calcagno, Andrew Paquette, Arturo Sinclair, Bruce Hsu, Markus Mueller,
Susan Aldworth, Nando Eweg, producer Robert Allen and betatesters Aaron
Hartman, Lee Cheramie and Andy Lehrfeld. The full list is in the game's About
screen.

**OpenTyrian:** The OpenTyrian Development Team and contributors: Carl Reinke,
Yuri Kunde Schlesner, mvcixT, syntaxglitch, Alex Rodionov, Stephan T. Lavavej,
emacs.hacker, Miro Kropacek, Alexandre Detiste, sweetlilmre, Sander van Dijk,
a.h.vandijk, qon, Adam Vandenberg, Dmitry Marakasov, Jan Alexander Steffens,
Jeff Smith, Kane, Pedro Catalão, Renato Alves, Sergii Pylypenko, George
Rawlinson, mcpusc, norm2782, okkirikomi, Patryk Obara and AshTR.

**OpenTyrian2000:** Kaito Sinclaire (KScl).

**Tyrian 2000+:** Gabriel Badolato, with help from Claude (Anthropic).

**Thanks to:** Sam Lantinga and the [SDL](https://www.libsdl.org/) team,
[camanis.net](https://www.camanis.net/tyrian/) for keeping the freeware data
available, and the Tyrian fans who kept the game alive.

## Links

- Tyrian 2000+: <https://github.com/gabrielbadolato/tyrian2000plus>
- Tyrian 2000+ on itch.io: <https://gabrielbadolato.itch.io/tyrian2000plus>
- OpenTyrian2000: <https://github.com/KScl/opentyrian2000>
- OpenTyrian: <https://github.com/opentyrian/opentyrian>
- Tyrian 2000 freeware data: <https://www.camanis.net/tyrian/>

## Building

- Windows x64 (from Linux): `./make_windows.sh`
- Linux x86_64 / Steam Deck: `./make_linux.sh`
- Tests: `tests/run_tests.sh [game-folder]`

The original OpenTyrian2000 README is in `doc/README-OpenTyrian2000`.
