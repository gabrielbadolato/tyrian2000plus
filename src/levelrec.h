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
#ifndef LEVELREC_H
#define LEVELREC_H

#include "opentyr.h"

#include "SDL.h"

#include <stdbool.h>

// What a player's controls did in one frame, after the keyboard, controller,
// mouse and Auto Fire were combined (the same values a network game sends).
typedef struct
{
	Sint16 dx, dy;          // the ship's move
	Sint16 accelX, accelY;
	Uint8 flags;            // LEVELREC_*
	float linkGunAngle;     // with LEVELREC_LINK_GUN_ANALOG
} LevelRecInput;

enum
{
	LEVELREC_BUTTONS = 0x0f,  // button[0..3]
	LEVELREC_LINK_GUN_ANALOG = 0x10,
	LEVELREC_FIRE_BUTTON_HELD = 0x20,  // fireButtonHeld (the special weapon)
	LEVELREC_AUTO_FIRING = 0x40,
	LEVELREC_FIRE_WAS_HELD = 0x80,
};

extern const char *const levelrec_filename;

// Title screen: Continue resumes the level that was being played when the
// game closed, if the save it would load is the one made when that level began.
bool levelrec_can_resume(int slot);
void levelrec_request_resume(bool request);
// Title screen: Replay shows the last level played again.
bool levelrec_can_watch(void);
bool levelrec_start_watch(void);

// JE_loadMap: a resumed or replayed level goes straight to its section of the
// episode instead of the ship menu (as when the player chose it there); 0 if not.
Uint8 levelrec_resume_section(void);

// The level loop (JE_main).
void levelrec_level_setup(void);   // before the random numbers of the level
void levelrec_level_begin(void);   // after the "LAST LEVEL" save
void levelrec_frame(void);         // at the top of every frame
bool levelrec_player_input(unsigned int p, LevelRecInput *input);
void levelrec_note_skip(void);     // the frame after F1 or the menu skips its end
bool levelrec_take_skip(void);
void levelrec_cheats_begin(void);  // around JE_mainKeyboardInput
void levelrec_cheats_end(void);
void levelrec_level_end(void);     // when the level loop ends
bool levelrec_end_watch(void);     // then: whether it was a Replay
void levelrec_close(void);         // when the game closes

bool levelrec_replaying(void);  // the input comes from the recording
bool levelrec_fast(void);       // replaying, unseen, up to where the game closed
bool levelrec_watching(void);   // Replay
bool levelrec_failed(void);     // the last replay came out different (for the tests)

// JE_starShowVGA while replaying fast: whether to show this frame, and the
// progress to draw on it.
bool levelrec_fast_frame_due(void);
void levelrec_draw_progress(SDL_Surface *surface);

#endif /* LEVELREC_H */
