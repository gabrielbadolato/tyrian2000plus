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
#ifndef VIDEO_H
#define VIDEO_H

#include "SDL.h"

#include <stdbool.h>

#define vga_width 320
#define vga_height 200

typedef enum {
	SCALE_CENTER,
	SCALE_INTEGER,
	SCALE_ASPECT_8_5,
	SCALE_ASPECT_4_3,
	ScalingMode_MAX
} ScalingMode;

extern const char *const scaling_mode_names[ScalingMode_MAX];

typedef struct
{
	int x, y, w, h;     // w = 0: nothing remembered yet
	bool has_position;  // false where the position is unknown (Wayland)
	bool maximized;
} WindowGeometry;

extern int fullscreen_display; // -1 means windowed
extern WindowGeometry window_geometry;
extern ScalingMode scaling_mode;
extern bool vsync_enabled;
extern bool sharp_bilinear_enabled;

void set_vsync(bool enabled);
void set_sharp_bilinear(bool enabled);

extern SDL_Surface *VGAScreen, *VGAScreenSeg;
extern SDL_Surface *game_screen;
extern SDL_Surface *VGAScreen2;

extern SDL_Window *main_window;
extern SDL_PixelFormat *main_window_tex_format;

void init_video(void);

void video_on_win_resize(void);
void video_remember_window(void);
void reinit_fullscreen(int new_display);
void toggle_fullscreen(void);
bool init_scaler(unsigned int new_scaler);
bool set_scaling_mode_by_name(const char *name);

void deinit_video(void);

void JE_clr256(SDL_Surface *);
// Shows `surface` instead of VGAScreen (smooth motion's pictures in between).
void video_present(SDL_Surface *surface);
// Smooth motion's finer pictures: how many times bigger than the game's screen
// the window shows it (1 to 4; 1 with a scaler other than None), and showing
// one (scale times the size of the screen, 8 bits per pixel, `pitch` bytes
// per row) where the screen goes.
int video_fine_scale(void);
void video_present_fine(const Uint8 *pixels, int pitch, int scale);
// How many times JE_showVGA was called (the mouse pointer of smooth motion).
extern unsigned int vga_shows;

void JE_showVGA(void);
void video_refresh(void);  // shows the last frame again (the touch controls changed)
void video_get_output_rect(SDL_Rect *out);  // where the game is in the window

void mapScreenPointToWindow(Sint32 *inout_x, Sint32 *inout_y);
void mapWindowPointToScreen(Sint32 *inout_x, Sint32 *inout_y);
void scaleWindowDistanceToScreen(Sint32 *inout_x, Sint32 *inout_y);

#endif /* VIDEO_H */
