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
#ifndef OSK_H
#define OSK_H

#include "keyboard.h"

#include "SDL.h"

#include <stdbool.h>

// On-screen keyboard for typing a name with the controller (save games and
// high scores).  It is shown below the game's name box while a controller is
// connected and only takes the controller's presses, so the real keyboard
// works as before.

#define OSK_Y 157  // top of the keyboard; the name boxes end above it

typedef enum
{
	OSK_NONE = 0,  // not for the keyboard: handle it as before
	OSK_HANDLED,   // moved the cursor or similar, nothing else to do
	OSK_CHAR,      // type the character
	OSK_DELETE,    // erase the last character
	OSK_ACCEPT,    // done (as Enter)
	OSK_CANCEL,    // leave without the name (as Esc)
} OskResult;

typedef struct
{
	int row, col;
	bool lowercase;
	bool saved;  // background below the keyboard was saved
	Uint8 background[320 * (200 - OSK_Y)];
} Osk;

void osk_init(Osk *osk, SDL_Surface *surface);
void osk_draw(Osk *osk, SDL_Surface *surface);
void osk_close(Osk *osk, SDL_Surface *surface);

// a key or click from the game's input queue; *ch gets the character for OSK_CHAR
OskResult osk_key(Osk *osk, const KeyboardInput *input, Uint8 *ch);
OskResult osk_mouse(Osk *osk, const MouseInput *input, Uint8 *ch);

#endif /* OSK_H */
