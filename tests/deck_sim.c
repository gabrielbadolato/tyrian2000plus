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

/* Usage: deck_sim <data-dir> <script> <shot-dir> [game args...]
 *
 * Runs the game as on a Steam Deck in Game Mode: SteamDeck=1, Steam's
 * virtual pad for the Deck's controls (28de:11ff), and a 1280x800 screen
 * (tests/run_deck_sim.sh starts one, with a window manager that makes the
 * window fill it, as Game Mode does).  The script presses the pad's buttons
 * and takes screenshots; each line is
 *     <ms to wait> press <button> | hold <button> <ms> | stick <x> <y> | shot <name> | quit
 * with button = a b x y back guide start ls rs lb rb up down left right
 * (lt and rt are the triggers).  Screenshots are PNGs of the whole screen. */

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int opentyrian_main(int argc, char *argv[]);

typedef struct { Uint32 wait; char cmd[16], arg[48]; int n1, n2; } Step;

static Step steps[512];
static int stepCount, nextStep;
static Uint32 stepAt;
static SDL_Joystick *pad;
static const char *shotDir;
static struct { int button, axis; Uint32 releaseAt; } held[8];

static const char *const buttonNames[SDL_CONTROLLER_BUTTON_MAX] = {
	"a", "b", "x", "y", "back", "guide", "start", "ls", "rs", "lb", "rb", "up", "down", "left", "right",
};

static void hold(const char *name, Uint32 ms)
{
	int button = -1, axis = -1;
	if (strcmp(name, "lt") == 0)
		axis = SDL_CONTROLLER_AXIS_TRIGGERLEFT;
	else if (strcmp(name, "rt") == 0)
		axis = SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
	for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
		if (buttonNames[i] != NULL && strcmp(name, buttonNames[i]) == 0)
			button = i;
	if (button < 0 && axis < 0)
	{
		fprintf(stderr, "deck_sim: unknown button '%s'\n", name);
		exit(EXIT_FAILURE);
	}

	if (button >= 0)
		SDL_JoystickSetVirtualButton(pad, button, 1);
	else
		SDL_JoystickSetVirtualAxis(pad, axis, 32767);

	for (size_t i = 0; i < sizeof held / sizeof *held; ++i)
		if (held[i].releaseAt == 0)
		{
			held[i].button = button;
			held[i].axis = axis;
			held[i].releaseAt = SDL_GetTicks() + ms;
			break;
		}
}

static void runScript(void)
{
	const Uint32 now = SDL_GetTicks();

	for (size_t i = 0; i < sizeof held / sizeof *held; ++i)
		if (held[i].releaseAt != 0 && now >= held[i].releaseAt)
		{
			if (held[i].button >= 0)
				SDL_JoystickSetVirtualButton(pad, held[i].button, 0);
			else
				SDL_JoystickSetVirtualAxis(pad, held[i].axis, -32768);
			held[i].releaseAt = 0;
		}

	while (nextStep < stepCount && now >= stepAt + steps[nextStep].wait)
	{
		const Step *s = &steps[nextStep++];
		stepAt = now;
		printf("deck_sim: %u ms: %s %s\n", now, s->cmd, s->arg);
		fflush(stdout);

		if (strcmp(s->cmd, "press") == 0)
			hold(s->arg, 120);
		else if (strcmp(s->cmd, "hold") == 0)
			hold(s->arg, s->n1);
		else if (strcmp(s->cmd, "stick") == 0)
		{
			SDL_JoystickSetVirtualAxis(pad, SDL_CONTROLLER_AXIS_LEFTX, s->n1);
			SDL_JoystickSetVirtualAxis(pad, SDL_CONTROLLER_AXIS_LEFTY, s->n2);
		}
		else if (strcmp(s->cmd, "shot") == 0)
		{
			// the whole (virtual) screen, as the player sees it
			char command[600];
			snprintf(command, sizeof command, "import -window root '%s/%s.png'", shotDir, s->arg);
			if (system(command) != 0)
				fprintf(stderr, "deck_sim: screenshot failed\n");
		}
		else if (strcmp(s->cmd, "quit") == 0)
			exit(EXIT_SUCCESS);
	}
}

int __real_SDL_PollEvent(SDL_Event *event);
int __wrap_SDL_PollEvent(SDL_Event *event);
int __wrap_SDL_OpenURL(const char *url);

int __wrap_SDL_PollEvent(SDL_Event *event)
{
	if (pad == NULL && SDL_WasInit(SDL_INIT_JOYSTICK))
	{
		SDL_VirtualJoystickDesc d;
		SDL_zero(d);
		d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
		d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
		d.naxes = SDL_CONTROLLER_AXIS_MAX;
		d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
		d.vendor_id = 0x28de;   // Valve
		d.product_id = 0x11ff;  // Steam's virtual pad, as the Deck's controls appear in Game Mode
		d.name = "Steam Virtual Gamepad";
		const int index = SDL_JoystickAttachVirtualEx(&d);
		pad = SDL_JoystickOpen(index);
		SDL_JoystickSetVirtualAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768);
		SDL_JoystickSetVirtualAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768);
		stepAt = SDL_GetTicks();
	}
	if (pad != NULL)
		runScript();

	return __real_SDL_PollEvent(event);
}

// No browser in a test.
int __wrap_SDL_OpenURL(const char *url)
{
	(void)url;
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 4)
	{
		fprintf(stderr, "usage: deck_sim <data-dir> <script> <shot-dir> [game args...]\n");
		return EXIT_FAILURE;
	}

	FILE *f = fopen(argv[2], "r");
	if (f == NULL)
	{
		perror(argv[2]);
		return EXIT_FAILURE;
	}
	char line[128];
	while (fgets(line, sizeof line, f) != NULL && stepCount < (int)(sizeof steps / sizeof *steps))
	{
		Step *s = &steps[stepCount];
		SDL_zerop(s);
		if (line[0] == '#' || sscanf(line, "%u %15s %47s %d %d", &s->wait, s->cmd, s->arg, &s->n1, &s->n2) < 2)
			continue;
		if (strcmp(s->cmd, "stick") == 0)
			sscanf(line, "%*u %*s %d %d", &s->n1, &s->n2);
		++stepCount;
	}
	fclose(f);
	shotDir = argv[3];

	SDL_setenv("SteamDeck", "1", 1);

	char *args[32] = { argv[0], "-t", argv[1] };
	int n = 3;
	for (int i = 4; i < argc && n < 31; ++i)
		args[n++] = argv[i];
	return opentyrian_main(n, args);
}
