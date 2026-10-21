/*
 * Tyrian 2000+
 * Copyright (C) 2026 Tyrian 2000+ contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Usage: test_autoplay <data-dir> <episode> <difficulty> [route] [section]
 *
 * Plays a whole episode without a screen, with the game's own "constant play"
 * test mode (the -c option: the ship flies and fires by itself and the item
 * screens are skipped), and stops when the episode ends.  The clock is virtual
 * (SDL_GetTicks and SDL_Delay are replaced at link time, see run_autoplay.sh), so
 * an episode takes seconds instead of an hour, and a level that never ends is
 * reported as a hang instead of blocking forever.  Build it with the
 * sanitizers to also catch memory errors.  [section] starts at another
 * section of levelsN.dat (to repeat a problem quickly). */

#include "config.h"
#include "editship.h"
#include "episodes.h"
#include "file.h"
#include "helptext.h"
#include "joystick.h"
#include "keyboard.h"
#include "lang.h"
#include "loudness.h"
#include "mtrand.h"
#include "mainint.h"
#include "nortsong.h"
#include "palette.h"
#include "params.h"
#include "player.h"
#include "smooth.h"
#include "sprite.h"
#include "tyrian2.h"
#include "varz.h"
#include "video.h"

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A level of the original game lasts a few minutes; 20 is plenty.
#define LEVEL_TIME_LIMIT_MS (20u * 60 * 1000)

static Uint32 virtualTicks = 0;
static Uint32 levelStartTicks = 0;
static unsigned int startEpisode;
static unsigned int levelsPlayed = 0;
static char currentLevel[sizeof levelName] = "";
static int currentMainLevel = -1;
static bool playing = false;
static unsigned int enemiesRemoved = 0;
static unsigned int route = 0;

static void finish(int status, const char *message)
{
	// smooth motion: every frame shown must come out the same when it is
	// redrawn from its list of drawings (src/smooth.c)
	unsigned long same, different, skipped;
	smooth_test_counts(&same, &different, &skipped);
	printf("smooth motion: %lu frames redrawn the same, %lu different, %lu not redrawn\n", same, different, skipped);
	unsigned long judder[16];
	smooth_test_judder(judder);
	printf("smooth motion: things whose speed zigzags: %lu backgrounds, %lu enemies, %lu enemy shots, %lu others\n",
	       judder[SMOOTH_BACKGROUND], judder[SMOOTH_ENEMY], judder[SMOOTH_ENEMY_SHOT],
	       judder[SMOOTH_PLAYER] + judder[SMOOTH_SIDEKICK] + judder[SMOOTH_PLAYER_SHOT] + judder[SMOOTH_EXPLOSION] +
	       judder[SMOOTH_REP_EXPLOSION] + judder[SMOOTH_STAR] + judder[SMOOTH_SUPERPIXEL]);
	if (different > 0 && status == 0)
	{
		status = EXIT_FAILURE;
		message = "smooth motion redrew a frame differently (a drawing primitive without SMOOTH_RECORD?)";
	}
	printf("%s: episode %u, difficulty %d, route %u, %u levels, %u s of game time, %u enemies removed by the autopilot%s%s\n",
	       status == 0 ? "ok" : "FAIL", startEpisode, initialDifficulty, route, levelsPlayed,
	       virtualTicks / 1000, enemiesRemoved, message ? ": " : "", message ? message : "");
	fflush(stdout);
	// exit() runs the game's atexit handlers too (log file, SDL_Quit).
	exit(status);
}

// Constant play always flies to the last planet the map offers.  With a route
// number n > 0, fly to the n-th one instead (or the last, if there are fewer),
// to also visit the other branches: put it last before the item screen reads
// it (JE_itemScreen is replaced at link time, like the clock).
void __real_JE_itemScreen(void);
void __wrap_JE_itemScreen(void);

void __wrap_JE_itemScreen(void)
{
	if (route > 0 && mapPNum >= 2 && mapPNum <= COUNTOF(mapSection))
	{
		const unsigned int choice = MIN(route, mapPNum) - 1;
		const JE_byte section = mapSection[choice];
		mapSection[choice] = mapSection[mapPNum - 1];
		mapSection[mapPNum - 1] = section;
	}

	__real_JE_itemScreen();
}

// Called from the virtual clock, so it runs all the time, in levels and menus.
static void watch(void)
{
	if (!playing)
		return;

	if (episodeNum != startEpisode)
		finish(EXIT_SUCCESS, NULL);  // the episode ended and the next one began

	if (mainLevel != currentMainLevel || strcmp(levelName, currentLevel) != 0)
	{
		if (strcmp(levelName, currentLevel) != 0)
		{
			printf("  level %-10s (section %u) at %u s\n", levelName, mainLevel, virtualTicks / 1000);
			fflush(stdout);
			++levelsPlayed;
		}
		SDL_strlcpy(currentLevel, levelName, sizeof currentLevel);
		currentMainLevel = mainLevel;
		levelStartTicks = virtualTicks;
	}

	if (virtualTicks - levelStartTicks > LEVEL_TIME_LIMIT_MS)
	{
		char message[128];
		snprintf(message, sizeof message, "level %s (section %u) did not end after %u minutes",
		         levelName, mainLevel, LEVEL_TIME_LIMIT_MS / 60000);
		finish(EXIT_FAILURE, message);
	}
}

Uint32 __wrap_SDL_GetTicks(void);
void __wrap_SDL_Delay(Uint32 ms);

Uint32 __wrap_SDL_GetTicks(void)
{
	watch();
	return virtualTicks;
}

// How long a level may stand still before the autopilot gives up on the
// enemies left on the screen.  Some levels scroll in a loop until the boss is
// shot down; past LONG_LEVEL_MS scrolling no longer counts as progress.
#define STALL_LIMIT_MS (60u * 1000)
#define LONG_LEVEL_MS (8u * 60 * 1000)

// The enemies the game counts as on the screen (see JE_drawEnemy); big ones
// can be mostly above it.
static bool onScreen(int i)
{
	return enemyAvail[i] == 0 && enemy[i].ex > -24 && enemy[i].ex < 296 && enemy[i].ey >= -112 && enemy[i].ey <= 190;
}

// The ship of constant play only sweeps from side to side, with the weapon it
// started with, while the level waits for some enemies to be shot down.  So
// fly under the lowest enemy, and when the level stands still for a minute
// (an enemy out of reach, or too tough), take the enemies off the screen the
// way the game does when they fly away, with the jump and the flags the level
// script expects from shooting them down.
static void pilot(void)
{
	static Uint32 lastProgressTicks = 0;
	static JE_word lastLoc = 0;

	if (lastProgressTicks < levelStartTicks)
		lastProgressTicks = levelStartTicks;

	// The bonus levels in Galaga style only end when the ship is lost; then
	// leave the game over screen, which constant play leaves out, as with a
	// key press (the game loads the save made before the bonus level).
	youAreCheating = !galagaMode;
	if (allPlayersGone && !endLevel)
		reallyEndLevel = true;

	if (endLevel)
	{
		lastProgressTicks = virtualTicks;
		return;
	}
	if (curLoc != lastLoc)
	{
		lastLoc = curLoc;
		if (virtualTicks - levelStartTicks < LONG_LEVEL_MS)
			lastProgressTicks = virtualTicks;
	}

	int target = -1;
	for (int i = 0; i < 100; ++i)
	{
		if (onScreen(i) && enemy[i].ey >= -24 && (target < 0 || enemy[i].ey > enemy[target].ey))
			target = i;
	}
	if (target >= 0)
		player[0].x = MIN(MAX(enemy[target].ex + enemy[target].mapoffset + 6, 40), 256);

	if (virtualTicks - lastProgressTicks > STALL_LIMIT_MS)
	{
		bool boss254 = false;
		for (int i = 0; i < 100; ++i)
		{
			if (onScreen(i))
			{
				// What shooting it down would change for the level script
				// (see the player shots in JE_main).
				if (enemy[i].linknum == 254)
					boss254 = true;
				if (enemy[i].special)
					globalFlags[enemy[i].flagnum - 1] = enemy[i].setto;

				enemyAvail[i] = 1;
				++enemiesRemoved;
			}
		}
		if (boss254 && superEnemy254Jump > 0)
			JE_eventJump(superEnemy254Jump);
		lastProgressTicks = virtualTicks;
	}
}

void __wrap_SDL_Delay(Uint32 ms)
{
	virtualTicks += ms ? ms : 1;
	watch();
	if (playing)
		pilot();
}

int main(int argc, char *argv[])
{
	if (argc < 4 || argc > 6)
	{
		fprintf(stderr, "usage: %s <data-dir> <episode 1-5> <difficulty> [route] [section]\n", argv[0]);
		return EXIT_FAILURE;
	}

	customDataDirPath = argv[1];
	startEpisode = atoi(argv[2]);
	const int difficulty = atoi(argv[3]);

	mt_srand(startEpisode * 100 + difficulty);  // the same run every time

	if (SDL_Init(0) != 0)
	{
		fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return EXIT_FAILURE;
	}
	atexit(SDL_Quit);

	loadConfiguration();
	lang_set(lang_setting);

	if (!findDataFiles())
	{
		fprintf(stderr, "no game data in %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	JE_loadHelpText();
	loadSaves();
	JE_scanForEpisodes();

	init_video();
	init_keyboard();
	init_joysticks();

	loadPals();
	JE_loadMainShapeTables("tyrian.shp");

	// Invincible, as with the C key in constant play: a dead ship would leave
	// the bosses alive and the level waiting for them forever.
	youAreCheating = true;
	smoothScroll = true;

	// record and redraw every frame, and draw a picture halfway between frames
	smooth_test_enable(true, 32768);

	if (init_audio())
		loadSndFile(false);

	loadExtraShapes();

	// What the title screen does before a new game.
	JE_initPlayerData();
	JE_sortHighScores();

	constantPlay = true;
	gameLoaded = true;
	onePlayerAction = false;
	twoPlayerMode = false;

	JE_initEpisode(startEpisode);
	initial_episode_num = episodeNum;
	difficultyLevel = initialDifficulty = difficulty;
	if (argc >= 5)
		route = atoi(argv[4]);
	if (argc >= 6)
		mainLevel = atoi(argv[5]);  // start at another section of levelsN.dat

	const ulong initial_cash[] = { 10000, 15000, 20000, 30000, 20000 };
	player[0].cash = initial_cash[startEpisode - 1];

	printf("episode %u, difficulty %d, route %u\n", startEpisode, difficulty, route);
	fflush(stdout);

	playing = true;
	JE_main();

	// JE_main only returns to the title screen.
	finish(EXIT_FAILURE, "back to the title screen before the episode ended");
}
