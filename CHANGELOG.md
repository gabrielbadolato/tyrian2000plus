# Changelog

What changed in each version of Tyrian 2000+. The numbers in parentheses are
the GitHub pull requests with the details of each change.

Tyrian 2000+ is a modified version of
[OpenTyrian2000](https://github.com/KScl/opentyrian2000). Since October 2026
its source files have been changed by this project; the full history of every
change is in this repository's git log.

## v1.2 (2026-10-20)

- **Statistics**: Setup > "Statistics..." shows the totals of all your games
  (time in levels, levels completed, enemies destroyed, deaths, secret levels
  and datacubes found, highest score) and the records of each level (best
  score, fastest time and most enemies destroyed). Beating a level's record
  shows "New level record!" on the end of level screen. They are kept in a
  file of their own (statistics.cfg), apart from the game's saves (#36).
- **Compare items in the shop**: in the ship's upgrade menu, each item shows
  a number next to its price, in green when it beats what your ship has and
  in red when it is weaker: damage per second for guns and sidekicks (at the
  power level shown), armor for ships, strength for shields and power for
  generators. The numbers come from the game's own data, which is not
  changed (#37).
- Portuguese: the power level under the weapon preview reads "NIVEL" and no
  longer runs into the upgrade price (#37).
- **Sturdier**: the whole game was played automatically, every episode on
  every difficulty and by both branches of the map, with memory checks on,
  and random keys were pressed through all the menus. It found one error,
  now fixed: the help line at the bottom of the ship menu read past the end
  of its text table (inherited from OpenTyrian2000). The tests are in
  `tests/run_autoplay.sh` for future versions (#38).
- **Automatic save at the ship**: the game's own automatic save, "Last Level",
  is now also written when you finish a level and arrive at the ship, as its
  help text promised. Continue (and Load Game > Last Level) bring you back to
  the ship with the level you just finished, even if the game was closed
  without saving. Your own save slots are never touched (#39).
- Two-player games no longer overwrite the one-player "Last Level" before a
  bonus game (#39).
- **The window remembers its size and place**: when the game is not
  fullscreen, it opens where it was last time, with the same size (or
  maximized). If that place is no longer on a screen, e.g. a monitor was
  unplugged, it opens centered. Leaving fullscreen with Alt-Enter also puts it
  back there (#39).
- Closing the window with its X button now keeps your settings, like Quit
  does (before, changes made in that session were lost) (#39).

## v1.1 (2026-10-20)

- **Continue** on the title screen: loads the game last saved or loaded
  (also the "LAST LEVEL" save the game makes when a level starts). It only
  appears when there is such a game.
- **More faithful music**: Setup > Sound > "Music Sound" chooses Accurate
  (the default), the Nuked OPL3 emulator, which sounds very close to the real
  Sound Blaster cards of 1995, or Classic, the emulator used until now. Both
  have about the same loudness.
- **Keyboard on the screen** for the controller: when saving a game or
  entering a high score with a controller connected, a keyboard appears below
  the name. The D-pad moves, A types, B erases, Start finishes and Back
  cancels. The real keyboard and the mouse work as before.
- **Vibration** option in Setup > Controls: Off, Weak or Strong (the
  default, as before). Choosing it gives a short sample.
- **Low battery warning**: when a wireless controller's battery gets low,
  a message appears once during the level; the Controller Map shows the
  battery level next to the controller's name.
- **Portuguese** (Brazilian): menus, help screens, item names and the
  program's own screens (Setup, pause, Controller Map, About) can be shown in
  Portuguese. Setup > "Language" chooses Automatic (the system's language,
  the default), English or Português. The game data stays unchanged: the
  translation is a separate table in the program, and the accented letters
  are drawn on top of the game's own fonts when they load (the two larger
  fonts show capitals without the accent, since they have no room above
  them). The data cube stories and the in-level texts are still in English.
- **Keyboard shortcuts** are listed in Setup > Controls > "Shortcuts":
  Alt+Enter (full screen), Esc (menu), P (pause), F1 (help) and F11
  (brightness). The README lists them too.
- **Detail level and game speed in Setup:** until now they were only in the
  pause menu. Graphics has "Detail Level" and Setup has "Game Speed", so they
  can be set before starting a game.
- **The ship menu's Joystick, Keyboard and Mouse items** now open the same
  screens as Setup > Controls (with button icons, Reset to Defaults and the
  Both Sidekicks action), instead of the old screens of the original game.
- The volume help in Setup > Sound says that Enter mutes.
- Long Setup menus close up a little so their last item no longer covers the
  description at the bottom.

## v1.0.5 (2026-10-20)

- **Controls in the pause menu** (#27): during a level, Start or Esc now has
  "Controls...", which opens the same Setup > Controls screens (keyboard,
  controller, buttons, mouse, Auto Fire and the Controller Map). Closing it
  goes back to the pause, and the level continues from where it stopped.
  Hidden in network games.
- **Button icons** (#25): with an Xbox-style (also Steam Controller), PlayStation,
  Nintendo Switch or Steam Deck controller, the help texts, the Controller Map
  and the controller menus show small pixel-art icons of the buttons instead
  of their names, and the Controller Map drawing shows them on its buttons.
  Its layout also follows the controller: the left stick above the D-pad on
  Xbox-style and Switch pads, a touchpad on PlayStation ones. The Steam Deck
  now shows its own names (L1, R1, L2, R2) instead of the Xbox ones.
- **Installers** (#24):
  - Windows: `Tyrian2000Plus-vX.Y.Z-windows-x64-setup.exe` installs the game
    for the current user (no administrator prompt), with a Start menu
    shortcut, an optional desktop shortcut and an uninstaller in
    Settings > Apps. Saves and settings are kept.
  - Steam Deck / SteamOS: `Tyrian2000Plus-vX.Y.Z-instalar-steam-deck.sh`
    installs the game and puts it in the Steam library as a non-Steam game
    (asking first, since Steam has to close for a few seconds). Existing
    installs get the Steam shortcut on their next update.
- **Own folder for saves and settings** (#24): `%APPDATA%\Tyrian2000Plus` on
  Windows and `~/.config/tyrian2000plus` on Linux, instead of
  OpenTyrian2000's folder, so the two games no longer share settings. The
  first start copies the saves and settings from the old folder and leaves
  it as it was. The portable mode is unchanged.
- **Both sidekicks on one button** (#26): a new controller action, "Both Sidekicks",
  fires the left and right sidekicks together, like the mouse option of the
  same name. By default it is on B (Circle on PlayStation, A on Switch), which
  had no use during a level; it can be changed in Setup > Controls >
  Controller... > Buttons... and in the ship's Joystick menu. Older
  `opentyrian.cfg` files get it too, unless B is already used there.
- **Fixes** (#28):
  - Steam Deck updater: it no longer stops halfway when the update replaces
    the updater itself (the copy from v1.0.4 still does once: if the game is
    not in the Steam library afterwards, run "Atualizar Tyrian 2000+" again).
  - Steam Deck updater: a release whose `SHA256SUMS` is not attached yet
    (the first minutes after it is published) is no longer installed
    unchecked; the updater asks to try again in a few minutes.
  - If copying the saves from OpenTyrian2000's folder fails, the game uses
    the old folder that time and tries the copy again on the next start,
    instead of starting with no saves.

## v1.0.4 (2026-10-19)

- **Fixes** (#19):
  - The game no longer crashes when a controller disconnects while
    Setup > Controls > Controller... is open.
  - In two-player games each player keeps their own controller (and rumble)
    when a controller leaves and comes back.
  - Odd sensitivity values in `opentyrian.cfg` no longer close the game.
  - Auto Fire no longer turns itself on or off after any pause, stops at the
    start of each level and stays off while recording a demo.
  - An unreadable `tyrian.sav` is kept as `tyrian.sav.bak` before it is
    replaced.
  - Controller Map: holding B to exit no longer goes back one menu too many,
    and long labels stay inside the screen.
  - Sharp Bilinear: a one-off failure no longer turns the saved option off.
- **Auto Fire for the special weapon too** (#20): with Auto Fire on, the
  special weapon fires again every second (except in network games).
- **Build security** (#21): every download the builds use (compiler, SDL,
  game data, butler) is checked by sha256; each release has a `SHA256SUMS`
  file, which `atualizar-tyrian.sh` checks before installing; GitHub Actions
  only writes to the repository in the final release step.
- **Repository cleanup** (#22):
  - Correct copyright in the files written for this project
    ("Tyrian 2000+ contributors"), also in the startup log.
  - The packages include `CODIGO-FONTE.txt` with the link to the exact source
    of that version, as the GPL asks (the itch.io ones too).
  - The Linux package also carries the licenses of SDL's internal parts
    (hidapi and yuv2rgb).
  - `Tyrian2000Plus.exe` shows its name and version in Properties > Details.
  - The main README is now in English (Portuguese in `README.pt-BR.md`), with
    screenshots, the right release file names and the right place of
    `opentyrian.log`.
  - The release packages leave out the original DOS programs of the game
    data (tyrian2k.exe, setup.exe etc.): the game never opens them, they do
    not run on modern Windows and antivirus tools may flag them. The data
    files themselves are still the originals, unchanged.
  - Removed leftovers from the parent project that do not apply here
    (`get_data.sh`, which fetched Tyrian 2.1, and the macOS build).

## v1.0.3 (2026-10-19)

- **Update notice** (#18): at startup the game asks GitHub whether there is a
  newer release. If there is, the title screen shows "vX is out! See About."
  and About shows the download links (itch.io and GitHub). It can be turned
  off in Setup > Check Updates.

## v1.0.2 (2026-10-19)

- **Auto Fire stays on** when the pause menu is closed with A (#15).
- **Automatic itch.io upload** (#16): publishing a GitHub release sends the
  packages to the itch.io page.
- **Steam Controller with Steam open** (#17): the workarounds were removed;
  the README explains what to do (hold Start for Steam's gamepad mode, close
  Steam, or start the game from Steam).

## v1.0.1 (2026-10-19)

- The packages carry the full README in Portuguese (`LEIA-ME.txt`) and in
  English (`README.txt`) (#14).

## v1.0 (2026-10-19)

First release.

- **Modern controllers** (#1): Xbox, PlayStation, Switch Pro, Steam
  Controller and others, with rumble and hotplug.
- **Auto Fire**: the fire button turns continuous fire on and off.
- **Setup > Controls** on the title screen: keyboard, controller, mouse and
  Auto Fire.
- **Controller button names** in the help screens.
- **Video**: Fit 4:3 by default, VSync, the Sharp Bilinear filter (#2) and
  Brightness; Graphics... and Auto Fire in the pause menu too (#7, #8).
- **Controller Map** (#5): a drawing of a controller showing what each button
  does.
- **Setup...** in the ship menu, and an automatic pause when the controller
  disconnects (#7).
- **2026 Steam Controller without Steam** on Windows (#9).
- **About screen** and READMEs in Portuguese and English (#11); the
  "Tyrian 2000+" name (#12).
- **Windows, Linux and Steam Deck builds** on GitHub Actions, with the game
  data in the release packages and `atualizar-tyrian.sh` for the Steam Deck
  (#1, #3, #13).
- **Tests** with virtual Xbox, PS5 and Switch Pro controllers (#6).
- Based on OpenTyrian as of October 2026 (new input, config, save, file and
  logging code).
