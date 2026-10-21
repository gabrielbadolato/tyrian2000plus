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
#ifndef SMOOTH_H
#define SMOOTH_H

#include "opentyr.h"

#include "SDL.h"

#include <stdbool.h>
#include <stdint.h>

// Smooth motion.  A level still runs at its own pace (35 frames per second),
// exactly as before; only the picture changes.  While the game waits for its
// next frame, the screen shows the moving things part of the way between
// where they were in the last frame and where they are now, at the display's
// refresh rate.
//
// How: during each frame of a level, every drawing primitive that writes to
// the playfield (game_screen and VGAScreen2) also appends what it drew to a
// list (smooth_record).  Redrawing that list over a copy of the playfield as
// it was when the last frame was shown gives the frame again, pixel for pixel
// (that is checked every frame; a frame that does not come out the same is
// shown as before, without smoothing).  The things that move are tagged with
// who they are (smooth_tag), so the list of the last frame says where each one
// was, and redrawing the list with each one moved back part of the way gives
// the pictures in between.  Nothing here reads or changes the game's state, so
// saves, the level recording, its checksums and the timing stay the same.

// The setting (Setup > Graphics > "Smooth Motion"; "smooth_motion" in the
// "video" section).
extern bool smooth_motion;

// Finer pictures in between (Setup > Graphics > "Fine Motion"; "smooth_fine"
// in the "video" section): drawn up to SMOOTH_FINE_MAX times bigger than the
// game's screen, as the window allows, so things also move by fractions of a
// pixel of the game.  Only with the "None" scaler.
extern bool smooth_fine;
#define SMOOTH_FINE_MAX 4

// Whether the drawing primitives record (checked inline, so the hooks cost
// nothing when smooth motion is off).
extern bool smooth_recording;

typedef enum
{
	SMOOTH_OP_SPRITE,             // blit_sprite
	SMOOTH_OP_SPRITE_BLEND,       // blit_sprite_blend
	SMOOTH_OP_SPRITE_HV_UNSAFE,   // blit_sprite_hv_unsafe
	SMOOTH_OP_SPRITE_HV,          // blit_sprite_hv
	SMOOTH_OP_SPRITE_HV_BLEND,    // blit_sprite_hv_blend
	SMOOTH_OP_SPRITE_DARK,        // blit_sprite_dark
	SMOOTH_OP_SPRITE2,            // blit_sprite2
	SMOOTH_OP_SPRITE2_CLIP,       // blit_sprite2_clip
	SMOOTH_OP_SPRITE2_BLEND,      // blit_sprite2_blend
	SMOOTH_OP_SPRITE2_DARKEN,     // blit_sprite2_darken
	SMOOTH_OP_SPRITE2_FILTER,     // blit_sprite2_filter
	SMOOTH_OP_SPRITE2_FILTER_CLIP,// blit_sprite2_filter_clip
	SMOOTH_OP_PIX,                // JE_pix
	SMOOTH_OP_RECTANGLE,          // JE_rectangle
	SMOOTH_OP_FILL,               // fill_rectangle_xy
	SMOOTH_OP_BAR_SHADE,          // JE_barShade
	SMOOTH_OP_BAR_BRIGHT,         // JE_barBright
	SMOOTH_OP_CLEAR,              // JE_clr256
	SMOOTH_OP_BACKGROUND,         // draw_background_* (one layer)
	SMOOTH_OP_DARKEN_BACKGROUND,  // JE_darkenBackground
	SMOOTH_OP_FILTER_HUE,         // JE_filterScreen
	SMOOTH_OP_FILTER_VALUE,
	SMOOTH_OP_LAVA,               // lava_filter (and the other filters: dst, src)
	SMOOTH_OP_WATER,
	SMOOTH_OP_ICED_BLUR,
	SMOOTH_OP_BLUR,
	SMOOTH_OP_STAR,               // a star of the starfield
	SMOOTH_OP_SUPERPIXEL,         // a superpixel (sparks)
	SMOOTH_OP_COUNT
} SmoothOp;

// Who the next draws belong to, for the pictures in between.  Untagged draws
// stay where they are (text, warnings, effects that cover the screen).
typedef enum
{
	SMOOTH_NONE,
	SMOOTH_BACKGROUND,    // id: layer
	SMOOTH_ENEMY,         // id: slot in enemy[]; part: offset in the enemy
	SMOOTH_PLAYER,        // id: player
	SMOOTH_SIDEKICK,      // id: player * 2 + side
	SMOOTH_PLAYER_SHOT,   // id: slot in playerShotData[]
	SMOOTH_ENEMY_SHOT,    // id: slot in enemyShot[]
	SMOOTH_EXPLOSION,     // id: slot in explosions[]
	SMOOTH_REP_EXPLOSION, // id: slot in rep_explosions[]
	SMOOTH_STAR,          // id: star
	SMOOTH_SUPERPIXEL,    // id: slot in superpixels[]
} SmoothTag;

void smooth_tag(SmoothTag tag, unsigned int id, unsigned int part);
void smooth_untag(void);

// The hook: what a primitive drew (dst, and src for the filters that read
// another surface), where, and with what.
void smooth_record(SmoothOp op, SDL_Surface *dst, SDL_Surface *src, int x, int y,
                   intptr_t a, intptr_t b, intptr_t c, intptr_t d);

#define SMOOTH_RECORD(op, dst, src, x, y, a, b, c, d) \
	do { if (smooth_recording) smooth_record(op, dst, src, x, y, (intptr_t)(a), (intptr_t)(b), (intptr_t)(c), (intptr_t)(d)); } while (0)

// Sprites were freed or reloaded: the drawings recorded so far may point to
// them, so this frame is shown without smoothing.
void smooth_forget_sprites(void);

// The level loop.
void smooth_level_begin(void);  // a level starts: forget the last one
void smooth_level_end(void);    // the level loop ends: stop recording
// JE_starShowVGA, before waiting for the frame to be due: shows the pictures
// in between until then, and returns true if it showed any (then the frame
// itself is not shown: the next pictures in between start from it), or false
// to show the frame as before.  view: 0 normal, 1 flipped, 2 headlight.
bool smooth_frame_due(int view);
// JE_starShowVGA, after the frame was shown (or not): start the next one.
void smooth_frame_shown(void);

// Outside the levels.  Until the frame started with setFrameCount(frames) is
// due, shows a picture per refresh of the display, each drawn and shown by
// show(alpha, data) with alpha (0..65536) how far the frame has gone when it
// shows.  Returns false (and shows nothing) if smooth motion is off or no
// picture fits before the frame is due; then show the frame as before.
bool smooth_show_until_due(int frames, void (*show)(Uint32 alpha, const void *data), const void *data);

// The weapon simulator of the ship menu (JE_weaponViewFrame): the stars and the
// shots are recorded like a level, on the screen, and the pictures in between
// keep what is drawn over them after.  begin: the simulator starts drawing a
// frame; drawn: it finished drawing what moves; show: instead of JE_showVGA,
// returns false to show the frame as before.
// The mouse pointer outside the levels (mouse_pointer_between): how long until
// a picture can be shown (ms; UINT32_MAX: smooth motion is off), and a picture
// was shown.
Uint32 smooth_picture_wait(void);
void smooth_picture_shown(void);
// the time between two pictures of the display (ms)
Uint32 smooth_refresh_ms(void);

void smooth_sim_begin(void);
void smooth_sim_drawn(void);
bool smooth_sim_show(int frames);

// For the tests: record and check every frame even with smooth motion off,
// and draw one picture in between at `alpha` (0..65536) each frame, without
// showing it.  Returns the counts of frames that came out the same and that
// did not.
void smooth_test_enable(bool enable, Uint32 alpha);
void smooth_test_counts(unsigned long *same, unsigned long *different, unsigned long *skipped);

#endif // SMOOTH_H
