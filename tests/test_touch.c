/*
 * Tyrian 2000+: Tyrian 2000 with modern controls, based on OpenTyrian2000
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/* Headless test for the touch screen controls (src/touch.c): the layout at
 * phone, tablet and PC sizes, and what fingers on the stick, the buttons and
 * the game turn into (level input, keys for the menus, mouse clicks). */
#include "keyboard.h"
#include "touch.h"
#include "video.h"

#include "SDL.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails = 0;

static void check(bool ok, const char *what)
{
	printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
	fails += !ok;
}

static int win_w, win_h;
static SDL_Rect game;
static TouchLayout layout;

static void finger(Uint32 type, SDL_FingerID id, float x, float y)
{
	SDL_Event e;
	memset(&e, 0, sizeof(e));
	e.type = type;
	e.tfinger.touchId = 1;
	e.tfinger.fingerId = id;
	e.tfinger.x = x / win_w;
	e.tfinger.y = y / win_h;
	touch_handle_event(&e);
}

// the game where video.c puts it in a window of this size (Fit 4:3)
static void fit(int w, int h)
{
	win_w = w;
	win_h = h;
	game.w = w * 3 > h * 4 ? h * 4 / 3 : w;
	game.h = w * 3 > h * 4 ? h : w * 3 / 4;
	game.x = (w - game.w) / 2;
	game.y = (h - game.h) / 2;
	touch_adjust_game_rect(w, h, &game);
	touch_layout(win_w, win_h, &game, &layout);
}

// counts the events waiting, by type, and empties the queue
static int count_events(Uint32 type, SDL_Scancode scancode)
{
	SDL_Event e;
	int n = 0;
	SDL_PumpEvents();
	while (SDL_PollEvent(&e))
		if (e.type == type && (type != SDL_KEYDOWN || e.key.keysym.scancode == scancode))
			++n;
	return n;
}

static bool overlap(const TouchCircle *a, const TouchCircle *b)
{
	return hypotf(a->x - b->x, a->y - b->y) < a->r + b->r;
}

static bool on_screen(const TouchCircle *c)
{
	return c->x - c->r >= 0 && c->y - c->r >= 0 && c->x + c->r <= win_w && c->y + c->r <= win_h;
}

static void check_layout(int w, int h)
{
	fit(w, h);

	bool ok = on_screen(&layout.stick);
	for (int b = 0; b < TOUCH_BUTTONS; ++b)
	{
		ok &= on_screen(&layout.button[b]) && !overlap(&layout.button[b], &layout.stick);
		for (int c = b + 1; c < TOUCH_BUTTONS; ++c)
			ok &= !overlap(&layout.button[b], &layout.button[c]);
	}
	char what[96];
	snprintf(what, sizeof(what), "%dx%d: controls on the screen, none on another", w, h);
	check(ok, what);
}

int main(void)
{
	SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
	init_video();
	init_keyboard();

	check(!touch_shown(), "hidden before the first touch");

	// the fingers in the test window (the dummy driver keeps its size)
	int w, h;
	SDL_GetWindowSize(main_window, &w, &h);
	JE_showVGA();
	fit(w, h);
	SDL_Rect shownGame;
	video_get_output_rect(&shownGame);
	check(SDL_RectEquals(&shownGame, &game), "the game where the test expects it");

	// a tap on the game: shows the controls, and is a click
	count_events(0, 0);
	finger(SDL_FINGERDOWN, 1, win_w / 2.0f, win_h / 2.0f);
	check(touch_shown(), "the first touch shows the controls");
	finger(SDL_FINGERUP, 1, win_w / 2.0f, win_h / 2.0f);
	SDL_Event e;
	int down = 0, up = 0;
	bool fromTouch = true;
	while (SDL_PollEvent(&e))
	{
		if (e.type == SDL_MOUSEBUTTONDOWN) { ++down; fromTouch &= e.button.which == SDL_TOUCH_MOUSEID; }
		if (e.type == SDL_MOUSEBUTTONUP) ++up;
	}
	check(down == 1 && up == 1 && fromTouch, "a tap on the game is one click");

	// the fire button: held while the finger is on it, no click
	const TouchCircle *fire = &layout.button[TOUCH_FIRE];
	TouchGameInput in;
	finger(SDL_FINGERDOWN, 2, fire->x, fire->y);
	touch_game_input(&in);
	check(in.fire && !in.sidekicks && count_events(SDL_MOUSEBUTTONDOWN, 0) == 0, "fire held, and no click");
	finger(SDL_FINGERUP, 2, fire->x, fire->y);
	touch_game_input(&in);
	check(!in.fire, "fire let go");

	// the stick, with fire at the same time (two fingers)
	finger(SDL_FINGERDOWN, 3, layout.stick.x, layout.stick.y);
	finger(SDL_FINGERDOWN, 4, fire->x, fire->y);
	finger(SDL_FINGERMOTION, 3, layout.stick.x + layout.stick.r, layout.stick.y);
	touch_game_input(&in);
	check(fabsf(in.x - 1) < 0.01f && fabsf(in.y) < 0.01f && in.fire, "stick all the way right while firing");
	finger(SDL_FINGERMOTION, 3, layout.stick.x, layout.stick.y - layout.stick.r * 0.1f);
	touch_game_input(&in);
	check(in.x == 0 && in.y == 0, "a small push is in the dead zone");
	finger(SDL_FINGERUP, 3, layout.stick.x, layout.stick.y);
	finger(SDL_FINGERUP, 4, fire->x, fire->y);

	// the stick floats: it centers where the finger lands in its zone
	const float zx = layout.stickZone.x + layout.stickZone.w * 0.5f, zy = layout.stickZone.y + layout.stickZone.h * 0.5f;
	finger(SDL_FINGERDOWN, 5, zx, zy);
	finger(SDL_FINGERMOTION, 5, zx, zy + layout.stick.r);
	touch_game_input(&in);
	check(fabsf(in.y - 1) < 0.01f && fabsf(in.x) < 0.01f, "the stick centers under the finger");
	finger(SDL_FINGERUP, 5, zx, zy);

	// in a level, the menu and mode buttons are presses
	const TouchCircle *menu = &layout.button[TOUCH_MENU];
	finger(SDL_FINGERDOWN, 6, menu->x, menu->y);
	finger(SDL_FINGERUP, 6, menu->x, menu->y);
	touch_game_input(&in);
	check(in.menu_pressed, "menu pressed in a level");
	touch_game_input(&in);
	check(!in.menu_pressed, "... only once");
	count_events(0, 0);
	touch_push_as_keyboard();
	check(count_events(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE) == 0, "... and not again as Esc in the menu that opens");

	// in the menus: fire = Enter, menu = Esc, the stick = arrows
	finger(SDL_FINGERDOWN, 7, fire->x, fire->y);
	finger(SDL_FINGERUP, 7, fire->x, fire->y);
	touch_push_as_keyboard();
	check(count_events(SDL_KEYDOWN, SDL_SCANCODE_RETURN) == 1, "fire is Enter in the menus");
	finger(SDL_FINGERDOWN, 8, menu->x, menu->y);
	finger(SDL_FINGERUP, 8, menu->x, menu->y);
	touch_push_as_keyboard();
	check(count_events(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE) == 1, "menu is Esc in the menus");
	finger(SDL_FINGERDOWN, 9, layout.stick.x, layout.stick.y);
	finger(SDL_FINGERMOTION, 9, layout.stick.x, layout.stick.y + layout.stick.r);
	touch_push_as_keyboard();
	check(count_events(SDL_KEYDOWN, SDL_SCANCODE_DOWN) == 1, "stick down is the down arrow");
	touch_push_as_keyboard();
	check(count_events(SDL_KEYDOWN, SDL_SCANCODE_DOWN) == 0, "... once, until it repeats");
	SDL_Delay(400);
	touch_push_as_keyboard();
	check(count_events(SDL_KEYDOWN, SDL_SCANCODE_DOWN) == 1, "... and it repeats while held");
	finger(SDL_FINGERUP, 9, layout.stick.x, layout.stick.y);

	// a real key hides the controls (the pushed ones, sym 0, do not)
	memset(&e, 0, sizeof(e));
	e.type = SDL_KEYDOWN;
	e.key.keysym.scancode = SDL_SCANCODE_RETURN;
	SDL_PushEvent(&e);
	handleSdlEvents();
	check(touch_shown(), "keys pushed for the touch controls keep them");
	e.key.keysym.sym = SDLK_RETURN;
	SDL_PushEvent(&e);
	handleSdlEvents();
	check(!touch_shown(), "a key on the keyboard hides them");

	// standing up: the game goes to the top, the controls below it
	finger(SDL_FINGERDOWN, 10, 10, 10);
	finger(SDL_FINGERUP, 10, 10, 10);
	fit(390, 844);
	check(game.y == 0 && layout.stick.y - layout.stick.r >= game.y + game.h, "portrait: the game at the top, the stick below it");
	fit(844, 390);
	check(game.x > 0 && game.h == win_h && layout.stick.x + layout.stick.r <= game.x + 1 &&
	      layout.button[TOUCH_FIRE].x - layout.button[TOUCH_FIRE].r >= game.x + game.w - 1,
	      "landscape phone: the stick and fire in the bars beside the game");

	const int sizes[][2] = { { 844, 390 }, { 390, 844 }, { 667, 375 }, { 375, 667 }, { 1280, 800 }, { 1024, 768 }, { 768, 1024 }, { 640, 400 } };
	for (size_t i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i)
		check_layout(sizes[i][0], sizes[i][1]);

	printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails != 0;
}
