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

/* Usage: test_monkey <data-dir> <seed> <minutes>
 *
 * Runs the whole game, from the title screen, without a screen, and presses
 * random keys (arrows, Enter, Esc, Space, letters) for <minutes> minutes of
 * game time (or 15 real minutes), to find menus and screens that crash or hang.  The clock is
 * virtual, as in test_autoplay.c, and the same seed presses the same keys.
 * The Quit item of the title screen shows the title screen again. */

#include "opentyr.h"

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>

int opentyrian_main(int argc, char *argv[]);

static Uint32 virtualTicks = 0;
static Uint32 timeLimit;
static Uint32 nextKeyTicks = 0;
static unsigned int pollsSinceKey = 0;
static unsigned int keysPressed = 0;
static SDL_Scancode heldKey = SDL_SCANCODE_UNKNOWN;
static Uint32 rng;
static unsigned int gamesStarted = 0;
static unsigned int quitsIgnored = 0;

static Uint32 nextRandom(void)
{
	// xorshift32: the same keys on every platform
	rng ^= rng << 13;
	rng ^= rng >> 17;
	rng ^= rng << 5;
	return rng;
}

static void report(void)
{
	printf("ok: %u keys in %u s of game time, %u games started, Quit chosen %u times\n",
	       keysPressed, virtualTicks / 1000, gamesStarted, quitsIgnored);
	fflush(stdout);
}

Uint32 __real_SDL_GetTicks(void);

// Also stop after 15 real minutes: some scalers (hq4x) are slow with the
// sanitizers.
static void checkTime(void)
{
	if (virtualTicks > timeLimit || __real_SDL_GetTicks() > 15 * 60 * 1000)
		exit(EXIT_SUCCESS);  // report() runs at exit
}

Uint32 __wrap_SDL_GetTicks(void);
void __wrap_SDL_Delay(Uint32 ms);
int __real_SDL_PollEvent(SDL_Event *event);
int __wrap_SDL_PollEvent(SDL_Event *event);
int __wrap_SDL_OpenURL(const char *url);
void __wrap_update_check_start(void);
bool __real_titleScreen(void);
bool __wrap_titleScreen(void);
void __real_JE_main(void);
void __wrap_JE_main(void);

Uint32 __wrap_SDL_GetTicks(void)
{
	checkTime();
	return virtualTicks;
}

void __wrap_SDL_Delay(Uint32 ms)
{
	virtualTicks += ms ? ms : 1;
	checkTime();
}

static SDL_Scancode randomKey(void)
{
	static const SDL_Scancode keys[] =
	{
		SDL_SCANCODE_UP, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN, SDL_SCANCODE_DOWN,
		SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
		SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN, SDL_SCANCODE_RETURN,
		SDL_SCANCODE_ESCAPE, SDL_SCANCODE_SPACE, SDL_SCANCODE_BACKSPACE,
		SDL_SCANCODE_A, SDL_SCANCODE_Z, SDL_SCANCODE_1,
	};
	return keys[nextRandom() % (sizeof keys / sizeof *keys)];
}

// Every ~0.1 s of game time (or every 1000 polls, for loops that poll without
// waiting), release the key held or press a new one.
int __wrap_SDL_PollEvent(SDL_Event *event)
{
	++pollsSinceKey;
	if (event == NULL || (virtualTicks < nextKeyTicks && pollsSinceKey < 1000))
		return __real_SDL_PollEvent(event);

	pollsSinceKey = 0;
	nextKeyTicks = virtualTicks + 50 + nextRandom() % 100;

	SDL_zerop(event);
	if (heldKey != SDL_SCANCODE_UNKNOWN)
	{
		event->type = SDL_KEYUP;
		event->key.keysym.scancode = heldKey;
		heldKey = SDL_SCANCODE_UNKNOWN;
	}
	else
	{
		event->type = SDL_KEYDOWN;
		event->key.keysym.scancode = heldKey = randomKey();
		++keysPressed;
	}
	event->key.state = event->type == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
	event->key.keysym.sym = SDL_GetKeyFromScancode(event->key.keysym.scancode);
	return 1;
}

// No browser (About links) and no network (update check) in a test.
int __wrap_SDL_OpenURL(const char *url)
{
	(void)url;
	return 0;
}

void __wrap_update_check_start(void)
{
}

bool __wrap_titleScreen(void)
{
	while (!__real_titleScreen())
		++quitsIgnored;
	return true;
}

void __wrap_JE_main(void)
{
	++gamesStarted;
	__real_JE_main();
}

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		fprintf(stderr, "usage: %s <data-dir> <seed> <minutes>\n", argv[0]);
		return EXIT_FAILURE;
	}

	rng = (Uint32)strtoul(argv[2], NULL, 10) * 2654435761u + 1;
	timeLimit = (Uint32)atoi(argv[3]) * 60 * 1000;

	printf("seed %s, %s minutes\n", argv[2], argv[3]);
	fflush(stdout);
	atexit(report);

	char *gameArgv[] = { argv[0], "--data", argv[1], NULL };
	return opentyrian_main(3, gameArgv);
}
