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

/* The level recording (src/levelrec.c): what Continue does after the game
 * closed in the middle of a level, and Replay.  Run by tests/run_resume.sh.
 *
 *   test_resume <data-dir> record <episode> <difficulty> <frames> <out-dir> [seed] [players]
 *     Plays the episode (constant play, invincible, with a pilot that steers
 *     with the keyboard keys, and some random moves), and <frames> frames
 *     into each level copies the user folder (lastlevel.rec, tyrian.sav,
 *     opentyrian.cfg) to <out-dir>/NN, as if the game had closed there; then
 *     ends that level and goes on to the next.
 *   test_resume <data-dir> resume <frames-after> [out-dir]
 *     With such a copy as the user folder, does what Continue does: the
 *     level has to be replayed up to where the copy was made, every checksum
 *     the same, and then go on for <frames-after> frames (and then, with
 *     [out-dir], copy the user folder there again, to resume once more).
 *   test_resume <data-dir> watch
 *     With such a copy as the user folder, does what Replay does: the whole
 *     recording has to play back with the same checksums.
 *
 * The clock is virtual (SDL_GetTicks and SDL_Delay are replaced at link time),
 * so a level takes seconds. */

#include "config.h"
#include "editship.h"
#include "episodes.h"
#include "file.h"
#include "helptext.h"
#include "joystick.h"
#include "keyboard.h"
#include "lang.h"
#include "levelrec.h"
#include "loudness.h"
#include "lvlmast.h"
#include "mainint.h"
#include "mtrand.h"
#include "nortsong.h"
#include "palette.h"
#include "params.h"
#include "player.h"
#include "sprite.h"
#include "tyrian2.h"
#include "varz.h"
#include "video.h"

#include "SDL.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef enum { RECORD, RESUME, WATCH } TestMode;

static TestMode testMode;
static Uint32 virtualTicks = 0;
static unsigned int startEpisode;
static unsigned int framesPerLevel;
static const char *outDir;
static unsigned int copies = 0;
static unsigned int levelFrames = 0;   // frames of the current level
static unsigned int framesAfter = 0;
static bool wasFast = false, handedOver = false;
static unsigned int fastFrames = 0, liveFrames = 0, watchedFrames = 0;
static Uint32 rng = 1;
// TEST_MANUAL=1: constant play only between the levels; in them the pilot
// presses fire, the sidekicks and the rear mode, with Auto Fire on
static bool manual = false;

static void finish(int status, const char *message)
{
	printf("%s: %s%s%s\n", status == 0 ? "ok" : "FAIL",
	       testMode == RECORD ? "record" : testMode == RESUME ? "resume" : "watch",
	       message ? ": " : "", message ? message : "");
	fflush(stdout);
	exit(status);
}

static Uint32 nextRandom(void)
{
	rng = rng * 1103515245u + 12345u;
	return rng >> 16;
}

Uint32 __wrap_SDL_GetTicks(void);
void __wrap_SDL_Delay(Uint32 ms);

Uint32 __wrap_SDL_GetTicks(void)
{
	return virtualTicks;
}

void __wrap_SDL_Delay(Uint32 ms)
{
	virtualTicks += ms ? ms : 1;
}

// The pause that comes after a resumed level waits for a key.
void __real_JE_pauseGame(void);
void __wrap_JE_pauseGame(void);
void __wrap_JE_pauseGame(void)
{
}

static bool copyUserFile(const char *name, const char *dir)
{
	File in = userFileOpen(name, "rb");
	if (in.error)
		return false;
	const long length = fileGetLength(&in);
	char *data = malloc(length > 0 ? (size_t)length : 1);
	fileReadExactly(&in, data, (size_t)length);
	fileClose(&in);

	char path[1100];
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	FILE *out = fopen(path, "wb");
	if (out == NULL)
	{
		free(data);
		return false;
	}
	fwrite(data, 1, (size_t)length, out);
	fclose(out);
	free(data);
	return true;
}

static void snapshot(const char *dir)
{
	if (mkdir(dir, 0755) != 0 && errno != EEXIST)
		finish(EXIT_FAILURE, "cannot create the copy folder");

	// as if the game closed now (the file is written every frame)
	if (!copyUserFile(levelrec_filename, dir) || !copyUserFile("tyrian.sav", dir) || !copyUserFile("opentyrian.cfg", dir))
		finish(EXIT_FAILURE, "cannot copy the user folder");

	char path[1100];
	snprintf(path, sizeof(path), "%s/level.txt", dir);
	FILE *info = fopen(path, "w");
	fprintf(info, "%u %s (section %u, episode %u)\n", levelFrames, levelName, mainLevel, episodeNum);
	fclose(info);

	printf("  copy in %s: %s (section %u) at frame %u\n", dir, levelName, mainLevel, levelFrames);
	fflush(stdout);
}

// Steer under the lowest enemy with the arrow keys, moving now and then at
// random, so that the recording has all kinds of input.
static void pilot(void)
{
	int target = -1;
	for (int i = 0; i < 100; ++i)
	{
		if (enemyAvail[i] == 0 && enemy[i].ey >= -24 && enemy[i].ey < 190 &&
		    (target < 0 || enemy[i].ey > enemy[target].ey))
			target = i;
	}

	const int x = target >= 0 ? enemy[target].ex + enemy[target].mapoffset + 6 : 150;
	const Uint32 r = nextRandom();
	keysactive[keySettings[KEY_SETTING_LEFT]] = player[0].x > x + 4 || r % 13 == 0;
	keysactive[keySettings[KEY_SETTING_RIGHT]] = player[0].x < x - 4 || r % 17 == 0;
	keysactive[keySettings[KEY_SETTING_UP]] = r % 7 == 0;
	keysactive[keySettings[KEY_SETTING_DOWN]] = r % 11 == 0;

	if (manual)
	{
		const Uint32 r2 = nextRandom();
		keysactive[keySettings[KEY_SETTING_FIRE]] = r2 % 23 == 0;  // toggles Auto Fire
		keysactive[keySettings[KEY_SETTING_LEFT_SIDEKICK]] = r2 % 5 == 0;
		keysactive[keySettings[KEY_SETTING_RIGHT_SIDEKICK]] = r2 % 7 == 0;
		keysactive[keySettings[KEY_SETTING_CHANGE_FIRE]] = r2 % 97 == 0;
	}
}

void __real_levelrec_frame(void);
void __wrap_levelrec_frame(void);
void __wrap_levelrec_frame(void)
{
	__real_levelrec_frame();
	++levelFrames;

	switch (testMode)
	{
	case RECORD:
		if (episodeNum != startEpisode)
			finish(EXIT_SUCCESS, NULL);
		pilot();
		if (levelFrames == framesPerLevel)
		{
			char dir[1024];
			snprintf(dir, sizeof(dir), "%s/%02u", outDir, ++copies);
			snapshot(dir);

			// on to the next level, as the skip-level keys do
			levelTimer = true;
			levelTimerCountdown = 0;
			endLevel = true;
			levelEnd = 40;
		}
		if (levelFrames > 20 * 60 * 35)
			finish(EXIT_FAILURE, "a level did not end");
		break;

	case RESUME:
		if (levelrec_failed())
			finish(EXIT_FAILURE, "the replay came out different");
		if (levelrec_fast())
		{
			wasFast = true;
			++fastFrames;
		}
		else if (wasFast && !handedOver)
		{
			handedOver = true;
			printf("  replayed %u frames of %s, then played on\n", fastFrames, levelName);
			fflush(stdout);
		}
		if (handedOver)
		{
			pilot();
			// if the level ended meanwhile, play a little of the next one
			// (its recording needs two checks to be resumable)
			if (++liveFrames >= framesAfter && levelFrames > 2 * 35)
			{
				if (outDir != NULL)
					snapshot(outDir);
				finish(EXIT_SUCCESS, NULL);
			}
		}
		break;

	case WATCH:
		if (levelrec_watching())
			++watchedFrames;
		break;
	}
}

void __real_levelrec_level_begin(void);
void __wrap_levelrec_level_begin(void);
void __wrap_levelrec_level_begin(void)
{
	__real_levelrec_level_begin();
	levelFrames = 0;
	if (manual)
		constantPlay = false;

	if (testMode == RESUME && !levelrec_fast() && !handedOver)
		finish(EXIT_FAILURE, "the level was not resumed");
}

void __real_levelrec_level_end(void);
void __wrap_levelrec_level_end(void);
void __wrap_levelrec_level_end(void)
{
	__real_levelrec_level_end();
	constantPlay = true;  // past the screens between the levels

	// a new special weapon, sidekicks and power for the next level, as if
	// bought in the shop
	if (manual && testMode == RECORD)
	{
		player[0].items.special = 1 + nextRandom() % 19;  // the ones sold in the shops
		player[0].items.sidekick[LEFT_SIDEKICK] = 1 + nextRandom() % OPTION_NUM;
		player[0].items.sidekick[RIGHT_SIDEKICK] = 1 + nextRandom() % OPTION_NUM;
		player[0].items.weapon[FRONT_WEAPON].power = 1 + nextRandom() % 5;
		player[0].items.weapon[REAR_WEAPON].power = 1 + nextRandom() % 5;
	}
}

int main(int argc, char *argv[])
{
	if (argc < 3)
	{
		fprintf(stderr, "usage: %s <data-dir> record|resume|watch ...\n", argv[0]);
		return EXIT_FAILURE;
	}

	customDataDirPath = argv[1];
	const char *modeName = argv[2];
	if (strcmp(modeName, "record") == 0 && argc >= 7)
	{
		testMode = RECORD;
		startEpisode = atoi(argv[3]);
		framesPerLevel = atoi(argv[5]);
		outDir = argv[6];
		rng = argc >= 8 ? (Uint32)atoi(argv[7]) : 1;
	}
	else if (strcmp(modeName, "resume") == 0 && argc >= 4)
	{
		testMode = RESUME;
		framesAfter = atoi(argv[3]);
		outDir = argc >= 5 ? argv[4] : NULL;
	}
	else if (strcmp(modeName, "watch") == 0)
	{
		testMode = WATCH;
	}
	else
	{
		fprintf(stderr, "bad arguments\n");
		return EXIT_FAILURE;
	}

	mt_srand(12345);

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

	smoothScroll = true;

	if (init_audio())
		loadSndFile(false);

	loadExtraShapes();

	// What the title screen does before a new game.
	JE_initPlayerData();
	JE_sortHighScores();

	constantPlay = true;
	manual = getenv("TEST_MANUAL") != NULL && strcmp(getenv("TEST_MANUAL"), "1") == 0;
	if (manual)
		autoFireToggle = true;

	if (testMode == RECORD)
	{
		// Invincible, as with the C key in constant play.
		youAreCheating = true;

		const int difficulty = atoi(argv[4]);
		gameLoaded = true;
		onePlayerAction = false;
		twoPlayerMode = argc >= 9 && atoi(argv[8]) == 2;
		if (twoPlayerMode)
		{
			player[0].items.ship = 11;  // Silver Ship, as a new two-player game
			inputDevice[0] = 1;
			inputDevice[1] = 2;
		}

		JE_initEpisode(startEpisode);
		initial_episode_num = episodeNum;
		difficultyLevel = initialDifficulty = difficulty;

		const ulong initial_cash[] = { 10000, 15000, 20000, 30000, 20000 };
		player[0].cash = initial_cash[startEpisode - 1];

		printf("record: episode %u, difficulty %d, %u frames per level%s\n",
		       startEpisode, difficulty, framesPerLevel, twoPlayerMode ? ", two players" : "");
		fflush(stdout);

		JE_main();
		finish(EXIT_FAILURE, "back to the title screen before the episode ended");
	}
	else if (testMode == RESUME)
	{
		// what Continue on the title screen does
		const int slot = lastSaveSlot;
		if (!levelrec_can_resume(slot))
			finish(EXIT_FAILURE, "Continue would not resume the level");

		levelrec_request_resume(true);
		performSave = false;
		JE_operation((JE_byte)slot);
		if (!gameLoaded)
			finish(EXIT_FAILURE, "the save did not load");

		JE_main();
		if (!handedOver)
			finish(EXIT_FAILURE, "the level was not resumed");
		finish(EXIT_SUCCESS, "the level ended after it was resumed");
	}
	else
	{
		if (!levelrec_can_watch())
			finish(EXIT_FAILURE, "Replay is not offered");
		if (!levelrec_start_watch())
			finish(EXIT_FAILURE, "the replay did not start");

		JE_main();

		if (levelrec_failed())
			finish(EXIT_FAILURE, "the replay came out different");

		char message[64];
		snprintf(message, sizeof(message), "%u frames", watchedFrames);
		finish(EXIT_SUCCESS, message);
	}
}
