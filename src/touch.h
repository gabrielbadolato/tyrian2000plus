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
#ifndef TOUCH_H
#define TOUCH_H

#include "SDL.h"

#include <stdbool.h>

// Touch screen controls (phones and tablets, in the browser; later Android):
// a stick and buttons drawn around the game, shown after the first touch and
// hidden again when a key or a controller is used.  Fingers on the game
// itself act as the mouse (tap = click), so the menus work by tapping.

enum
{
	TOUCH_FIRE = 0,
	TOUCH_SIDEKICKS,
	TOUCH_MODE,
	TOUCH_MENU,
	TOUCH_BUTTONS
};

typedef struct
{
	float x, y, r;  // center and radius, in window pixels
} TouchCircle;

typedef struct
{
	SDL_Rect stickZone;      // where a finger takes the stick
	TouchCircle stick;       // the stick's base when no finger holds it
	TouchCircle button[TOUCH_BUTTONS];
} TouchLayout;

void touch_init(void);

// from handleSdlEvents: SDL_FINGERDOWN/UP/MOTION
void touch_handle_event(const SDL_Event *ev);

bool touch_shown(void);
void touch_hide(void);  // a key or a controller was used

// true when the controls changed and the screen should show it
bool touch_take_redraw(void);

// in portrait, the game moves to the top to leave room for the controls
void touch_adjust_game_rect(int win_w, int win_h, SDL_Rect *game);
void touch_layout(int win_w, int win_h, const SDL_Rect *game, TouchLayout *out);

void touch_draw(SDL_Renderer *renderer, int win_w, int win_h, const SDL_Rect *game);
void touch_release_textures(void);  // before the renderer goes away

// menus: the stick and buttons as arrow keys, Enter and Esc
void touch_push_as_keyboard(void);

// levels: stick (-1..1), held buttons, and presses since the last call
typedef struct
{
	float x, y;
	bool fire, sidekicks;
	bool mode_pressed, menu_pressed;
} TouchGameInput;

bool touch_game_input(TouchGameInput *out);

#endif /* TOUCH_H */
