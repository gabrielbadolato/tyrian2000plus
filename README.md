# Tyrian 2000+

[Português](README.pt-BR.md) | **English**

Tyrian 2000 with modern controllers, for Windows, Linux and the Steam Deck.

**A free, unofficial fan project.** It is not made, sold or endorsed by
Eclipse Software, Jason Emery or the original publishers. The game data is the
original freeware release, unchanged.

**[⬇ Download (GitHub Releases)](https://github.com/gabrielbadolato/tyrian2000plus/releases/latest)** ·
**[⬇ Download on itch.io](https://gabrielbadolato.itch.io/tyrian2000plus)**

Free, with the original game data included: Windows installer or .zip,
and Linux / Steam Deck. How to install is [below](#installation).
Or play it right in the browser on the itch.io page, with nothing to
install, also on a phone or tablet (on-screen controls). Your saves stay in
that browser, and you can take them to the computer or back (see
[Saved games](#saved-games-between-computers-and-the-browser)).

<table>
  <tr>
    <td><img src="doc/screenshots/1-tela-titulo.png" width="400" alt="Title screen with Continue"></td>
    <td><img src="doc/screenshots/2-fase.png" width="400" alt="A level"></td>
  </tr>
  <tr>
    <td><img src="doc/screenshots/3-loja.png" width="400" alt="Shop comparing each gun with the ship's (green is better)"></td>
    <td><img src="doc/screenshots/4-pausa-auto-fire-graphics.png" width="400" alt="Pause menu with Auto Fire, Controls and Graphics"></td>
  </tr>
  <tr>
    <td><img src="doc/screenshots/5-controller-map.png" width="400" alt="Controller Map with Xbox button icons"></td>
    <td><img src="doc/screenshots/6-estatisticas.png" width="400" alt="Statistics and level records"></td>
  </tr>
</table>

What changed in each version is in [CHANGELOG.md](CHANGELOG.md).

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
- Auto Fire, and a Vibration option (off, weak or strong).
- Controller button names in the help screens.
- A keyboard on the screen to type save and high score names with the
  controller, and a warning when the controller's battery is low.
- "Continue" on the title screen: back to the last game in one press, and
  back to the very point of a level if the game closed in the middle of it.
- "Replay Last Level" in Setup: watch the last level you played again.
- In the ship's upgrade menu, each item shows its damage (or armor, shield or
  power) in green when it beats what your ship has and in red when it is
  weaker.
- A second music sound to choose in Setup > Sound: the Nuked OPL3 emulator,
  close to a real 1995 Sound Blaster card (the classic sound stays the
  default).
- More graphics options, including in the pause menu, and a Sharp Bilinear
  filter.
- Smooth Motion (Setup > Graphics, on by default): the game still runs at
  35 frames per second, exactly as before, but the screen shows the movement
  in between at your monitor's refresh rate (60, 120, 144, 240 Hz...), in the
  levels, the fades between screens and the weapon preview of the ship menu.
  In the levels the pictures in between are drawn at up to 4x the game's
  resolution (by the size of the window, with the "None" scaler), so even a
  background that scrolls one pixel per frame glides in quarter pixels.
- Menus, help and item names in Portuguese (Brazilian).
- Ready-to-play builds for Windows, Linux and the Steam Deck.
- An About screen with the story of the project and the credits.
- A What's New screen with what changed in each version, in English or
  Portuguese, which also shows what a newer version brings before you update.

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

The easiest way is the installer: download
`Tyrian2000Plus-vX.Y.Z-windows-x64-setup.exe` and run it. It needs no
administrator rights, adds Tyrian 2000+ to the Start menu (and, if you leave
the box checked, to the desktop) and can be removed in Settings > Apps. Your
saves and settings stay in `%APPDATA%\Tyrian2000Plus` when you update or
uninstall. To update, run the installer of the new version.

Windows may show "Windows protected your PC" because the installer is not
signed: click "More info" > "Run anyway".

Or, without installing:

1. Download `Tyrian2000Plus-vX.Y.Z-windows-x64.zip` (vX.Y.Z is the version,
   for example v1.0.5) and extract it to a folder.
2. Extract `tyrian2000.zip` and copy **all** the files inside its `tyrian2000`
   folder (tyrian.hdt, tyrian1.lvl, music.mus etc.) into the `data` folder,
   next to `Tyrian2000Plus.exe`.
3. Plug in your controller and run `Tyrian2000Plus.exe`.

### Steam Deck and Linux

The easiest way is the installer script, which also updates the game later:

1. In Desktop Mode, download `Tyrian2000Plus-vX.Y.Z-instalar-steam-deck.sh`
   from the Releases page.
2. Open Konsole and run `bash ~/Downloads/Tyrian2000Plus-*-instalar-steam-deck.sh`.
3. It installs the game in `~/Jogos/Tyrian2000Plus` (with the game data),
   adds menu shortcuts and puts "Tyrian 2000+" in your Steam library. For
   that, Steam has to close for a few seconds; the script asks first and
   opens Steam again.
4. Go back to Game Mode and start Tyrian 2000+ from your library.

To update, use "Atualizar Tyrian 2000+" in the Desktop Mode menu (or run
`~/Jogos/Tyrian2000Plus/atualizar-tyrian.sh`).

Or by hand:

1. In Desktop Mode, extract `Tyrian2000Plus-vX.Y.Z-linux-x86_64.tar.gz` (right click >
   Extract here) to a folder, for example `/home/deck/Games`.
2. If the `data` folder is empty, copy the game data into it, as in step 2
   for Windows.
3. In Steam: "Add a Game" > "Add a Non-Steam Game" > "Browse", pick the
   `Tyrian2000Plus` file and confirm.
4. Go back to Game Mode and start Tyrian2000Plus from your library.

`Tyrian2000Plus.png` can be used as the icon in Steam. If you installed
through the itch.io app you don't need the script: the app updates itself.

**Deck screen:** the default is "Fit 4:3", true to 1995 monitors. For full
screen with sharp pixels, use Setup > Graphics > Scaling Mode > "Integer". The
game runs at 35 frames per second; Smooth Motion (on by default)
shows the movement at the screen's refresh rate, so any rate works. If you
turn it off, set the Deck OLED to 70 Hz (per-game profile) and leave the Deck
LCD at 60 Hz. Don't turn on Steam's frame limit.

## Controls

| Controller (Xbox)     | Action                              |
|-----------------------|-------------------------------------|
| Left stick or D-pad   | Move the ship                       |
| A or RT               | Fire                                |
| Y                     | Change the rear weapon mode         |
| LB or LT              | Left sidekick                       |
| RB or X               | Right sidekick                      |
| B                     | Both sidekicks at once              |
| Start (Menu)          | Game menu                           |
| Back (View)           | Pause                               |

In menus, A confirms and B goes back. On PlayStation and Switch Pro
controllers, the buttons in the same positions do the same thing. Buttons can
be changed in Setup > Controls > Controller... > Buttons.

Keyboard: arrows move, Space fires, Enter changes the rear weapon, Ctrl and
Alt fire the sidekicks. Keys that work anywhere (also listed in Setup >
Controls > Shortcuts):

| Key       | What it does                                    |
|-----------|-------------------------------------------------|
| Alt+Enter | Switch between full screen and a window         |
| Esc       | Open the menu during a level; go back in menus  |
| P         | Pause the game during a level                   |
| F1        | Help, in a level or in the ship menu            |
| F11       | Brightness, during a level                      |

In a window, the game opens where it was last time, with the same size (the
first time, as big as fits well on the screen).

**Touch screen** (phone or tablet, in the browser): touch the screen and a
stick and buttons show up beside the game (below it with the phone upright):
the stick moves, FIRE fires, SIDEKICKS fires both sidekicks, MODE changes the
rear weapon and MENU opens the game menu. You can also drag a finger on the
game itself: the ship follows it, firing. In menus, tap an item, or use the
stick and FIRE (OK) / MENU (back). A key or a controller hides the touch
controls again.

## Where each option is

- **Title screen > Setup:** Graphics, Sound,
  Controls, game speed, Language, Check Updates, Saved Games, Jukebox,
  Replay Last Level, Statistics, What's New and About.
- **Title screen > Continue:** loads the game you last saved or loaded (the
  game also saves "LAST LEVEL" when you arrive at the ship after a level and
  when a level starts). Shown when there is one. If the game closed in the
  middle of a level (the window closed, the Steam Deck turned off, a crash),
  Continue goes back to that point of the level and pauses there; it takes a
  few seconds, as the game replays the level unseen up to that point.
  Quitting a level from its menu gives the level up, as always.
- **Setup > Replay Last Level:** shows the last level you played again (it
  is recorded as you play, in lastlevel.rec in your user folder). Any key or
  button stops it. Shown when there is a recorded level, in the Setup of the
  title screen.
- **Setup > Sound > Music Sound:** "Classic" (the default, the
  emulator OpenTyrian always used) or "Accurate" (Nuked OPL3).
- **Setup > Controls:** keyboard, controller, mouse, Auto Fire, Vibration,
  the Controller Map (with the controller's battery level) and the list of
  keyboard shortcuts.
- **Typing a name with the controller** (saving a game, a high score): a
  keyboard appears below the name. The D-pad moves, A types, B erases, Start
  finishes and Back cancels.
- **Ship menu (between levels) > Upgrade Ship:** next to each item's price is
  a number to compare it with what your ship has, in green when it is better
  and in red when it is worse: damage per second for guns and sidekicks (at
  the power level shown; it counts only direct hits, so homing or piercing
  shots can do more), armor for ships, strength for shields and power for
  generators.
- **Ship menu (between levels) > Options > Setup...:** the same Setup. The
  Joystick, Keyboard and Mouse items there open the same screens as Setup >
  Controls.
- **During a level, Start or Esc:** volume, game speed, Auto
  Fire, Controls... (the same Setup > Controls) and Graphics... (full screen,
  scaling, Sharp Bilinear, VSync, Smooth Motion and brightness, applied right
  away).
- **Setup > Statistics:** the totals of all your games (time in levels,
  levels completed, enemies destroyed, deaths, secret levels, datacubes,
  highest score) and the records of each level: best score, fastest time and
  most enemies destroyed. Kept in statistics.cfg, next to the saves.
- **Setup > Saved Games:** on Windows and Linux, opens the folder with
  tyrian.sav (your saved games) and shows its path on the status line (on a
  Steam Deck in Game Mode there is no file manager, so the path is what you
  get); in the browser, downloads or loads it. See below.
- **Setup > About:** what the game is, where it came from, the credits and
  the links (Enter or A opens a link in your browser).
- **Setup > What's New** (also at the top of About): what changed in each
  version, newest first, in English or Portuguese. If a newer version is out,
  its notes are at the top, so you can see what it brings before updating.
  After an update, the title screen says "New in vX! See Setup." until you
  open it.
- **Setup > Language:** English or Portuguese (Brazilian) menus, help and
  item names. "Automatic" (the default) follows the system's language. The
  data cube stories are still in English.
- **Setup > Check Updates:** at startup the game asks GitHub for the latest
  published version. If there is a newer one, a note shows in the corner of
  the title screen, and About has the links to download it (itch.io and GitHub). The same
  answer brings the notes of each version for What's New, kept in
  release-notes.txt in your user folder for when there is no internet. None
  of your data is sent; without internet, nothing shows. Set it to "Off" to
  turn it off. In the browser the game never asks (that version is always
  the newest one on itch.io); the page itself fetches the notes for What's
  New.

## Saved games between computers and the browser

All your saved games and high scores are in one file, `tyrian.sav`, the same
in every version (it is even the original game's format).

- **From the browser to the computer:** in the browser, Setup > Saved
  Games... (or "Saved games" on the first screen, before Play) > "Download
  tyrian.sav". On the computer, with the game at the title screen (or
  closed), Setup > Saved Games... opens the game's folder: put the
  downloaded `tyrian.sav` there, replacing the one in it.
- **From the computer to the browser:** on the computer, Setup > Saved
  Games... opens the folder with `tyrian.sav`. In the browser, Setup > Saved
  Games... > "Load a tyrian.sav..." and pick that file. The browser keeps
  its previous saves as `tyrian-before-loading.sav` and opens the game again.

The game folder is `%APPDATA%\Tyrian2000Plus` on Windows and
`~/.config/tyrian2000plus` on Linux and the Steam Deck. A `tyrian.sav`
copied there while the game is open is read when you go back to the title
screen, and the game does not write over it when it closes.

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

The game writes `opentyrian.log` in the user folder, next to the saves and
the settings: `%APPDATA%\Tyrian2000Plus` on Windows (type that path in File
Explorer's address bar) and `~/.config/tyrian2000plus` on Linux and the Steam
Deck. Please send that file when reporting a problem.

If the game folder has an `opentyrian.cfg` file, all of this stays in the game
folder itself (portable mode). Up to v1.0.4 the user folder was the original
OpenTyrian2000's (`%APPDATA%\OpenTyrian2000`, `~/.config/opentyrian2000`):
the first time the game starts, it copies the saves and settings from there
and leaves that folder as it was.

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

- Windows x64 (from Linux): `./make_windows.sh`; installer (needs NSIS):
  `./make_installer.sh dist/Tyrian2000Plus-windows-x64 setup.exe`
- Linux x86_64 / Steam Deck: `./make_linux.sh`
- Tests: `tests/run_tests.sh [game-folder]`

The original OpenTyrian2000 README is in `doc/README-OpenTyrian2000`.
