/* 
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) The OpenTyrian Development Team
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
#ifndef MOUSE_H
#define MOUSE_H

#include "opentyr.h"

#include "SDL.h"

enum
{
	MOUSE_POINTER_NORMAL = 0,
	MOUSE_POINTER_UP,
	MOUSE_POINTER_DOWN,
	MOUSE_POINTER_LEFT,
	MOUSE_POINTER_RIGHT,
};

extern bool has_mouse;

extern bool mouseInactive;
extern JE_byte mouseCursor;

void JE_mouseStart(void);
void JE_mouseStartFilter(Uint8 filter);
void JE_mouseReplace(void);

// Smooth motion outside the levels: the pointer follows the mouse at the
// display's refresh rate while the game waits for its next frame.
// waitUntilElapsed: after a frame was shown with the pointer, shows the screen
// again with the pointer where the mouse is now, if it moved.
// Returns how long to wait (ms) before calling it again.
Uint32 mouse_pointer_between(void);
void mouse_pointer_forget(void);
// On a copy of the screen as it was shown (with the pointer): takes the
// pointer away / draws it where the mouse is now.
void mouse_pointer_remove(SDL_Surface *copy);
void mouse_pointer_draw_now(SDL_Surface *copy);

#endif /* MOUSE_H */
