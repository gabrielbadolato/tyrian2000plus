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
#include "mouse.h"

#include "keyboard.h"
#include "smooth.h"
#include "sprite.h"
#include "video.h"

#include <string.h>

#if defined(TARGET_GP2X) || defined(TARGET_DINGUX)
bool has_mouse = false;
#else
bool has_mouse = true;
#endif

bool mouseInactive = true;
JE_byte mouseCursor;

static JE_word mouseGrabX, mouseGrabY;
static JE_byte mouseGrabShape[24 * 28];

// Smooth motion: the pointer as the last frame showed it.
static bool pointerShown = false;
static unsigned int pointerShows = 0;
static Sint32 pointerX, pointerY;
static int pointerFilter = -1;
static SDL_Surface *pointerScreen = NULL;

static void JE_drawShapeTypeOne(SDL_Surface *surface, JE_word x, JE_word y, JE_byte *shape)
{
	JE_word xloop = 0, yloop = 0;
	JE_byte *p = shape; /* shape pointer */
	Uint8 *s;   /* screen pointer, 8-bit specific */
	Uint8 *s_limit; /* buffer boundary */

	s = (Uint8 *)surface->pixels;
	s += y * surface->pitch + x;

	s_limit = (Uint8 *)surface->pixels;
	s_limit += surface->h * surface->pitch;

	for (yloop = 0; yloop < 28; yloop++)
	{
		for (xloop = 0; xloop < 24; xloop++)
		{
			if (s >= s_limit)
				return;
			*s = *p;
			s++; p++;
		}
		s -= 24;
		s += surface->pitch;
	}
}

static void JE_grabShapeTypeOne(JE_word x, JE_word y, JE_byte *shape)
{
	JE_word xloop = 0, yloop = 0;
	JE_byte *p = shape; /* shape pointer */
	Uint8 *s;   /* screen pointer, 8-bit specific */
	Uint8 *s_limit; /* buffer boundary */

	s = (Uint8 *)VGAScreen->pixels;
	s += y * VGAScreen->pitch + x;

	s_limit = (Uint8 *)VGAScreen->pixels;
	s_limit += VGAScreen->h * VGAScreen->pitch;

	for (yloop = 0; yloop < 28; yloop++)
	{
		for (xloop = 0; xloop < 24; xloop++)
		{
			if (s >= s_limit)
				return;
			*p = *s;
			s++; p++;
		}
		s -= 24;
		s += VGAScreen->pitch;
	}
}

typedef struct
{
	Uint16 index;
	Uint8 x;
	Uint8 y;
	Uint8 w;
	Uint8 h;
	Uint8 fx;
	Uint8 fy;
} MousePointerSpriteInfo;

static const MousePointerSpriteInfo mousePointerSprites[] = // fka mouseCursorGr
{
	{ 273, 0, 0, 11, 16,  0,  0 },
	{ 275, 0, 0, 21, 16, 10,  8 },
	{ 277, 0, 0, 21, 16, 10,  7 },
	{ 279, 0, 0, 16, 21,  8, 10 },
	{ 281, 8, 0, 16, 21,  7, 10 },
};

void JE_mouseStart(void)  // FKA NewShape.mouseStart
{
	if (!has_mouse)
		return;

	const MousePointerSpriteInfo *spriteInfo = &mousePointerSprites[mouseCursor];

	mouseGrabX = MIN(MAX(spriteInfo->fx, mouseX), 320 - (spriteInfo->w - spriteInfo->fx)) - spriteInfo->fx;
	mouseGrabY = MIN(MAX(spriteInfo->fy, mouseY), 200 - (spriteInfo->h - spriteInfo->fy)) - spriteInfo->fy;

	JE_grabShapeTypeOne(mouseGrabX, mouseGrabY, mouseGrabShape);

	if (!mouseInactive)
	{
		const Sint32 x = mouseX - spriteInfo->x - spriteInfo->fx;
		const Sint32 y = mouseY - spriteInfo->y - spriteInfo->fy;
		blit_sprite2x2_clip(VGAScreen, x, y, shopSpriteSheet, spriteInfo->index);
	}

	pointerShows = vga_shows;
	pointerShown = false;
	pointerX = mouseX;
	pointerY = mouseY;
	pointerFilter = -1;
}

void JE_mouseStartFilter(Uint8 filter)
{
	if (!has_mouse)
		return;

	const MousePointerSpriteInfo *spriteInfo = &mousePointerSprites[mouseCursor];

	mouseGrabX = MIN(MAX(spriteInfo->fx, mouseX), 320 - (spriteInfo->w - spriteInfo->fx)) - spriteInfo->fx;
	mouseGrabY = MIN(MAX(spriteInfo->fy, mouseY), 200 - (spriteInfo->h - spriteInfo->fy)) - spriteInfo->fy;

	JE_grabShapeTypeOne(mouseGrabX, mouseGrabY, mouseGrabShape);

	if (!mouseInactive)
	{
		const Sint32 x = mouseX - spriteInfo->x - spriteInfo->fx;
		const Sint32 y = mouseY - spriteInfo->y - spriteInfo->fy;
		blit_sprite2x2_filter_clip(VGAScreen, x, y, shopSpriteSheet, spriteInfo->index, filter);
	}

	pointerShows = vga_shows;
	pointerShown = false;
	pointerX = mouseX;
	pointerY = mouseY;
	pointerFilter = filter;
}

void JE_mouseReplace(void)  // FKA NewShape.MouseReplace
{
	if (!has_mouse)
		return;

	JE_drawShapeTypeOne(VGAScreen, mouseGrabX, mouseGrabY, mouseGrabShape);

	// the screen was shown with the pointer: until the next frame, it can be
	// shown again with the pointer where the mouse is
	pointerShown = !mouseInactive && vga_shows != pointerShows;
}

void mouse_pointer_forget(void)
{
	pointerShown = false;
}

void mouse_pointer_remove(SDL_Surface *copy)
{
	if (has_mouse)
		JE_drawShapeTypeOne(copy, mouseGrabX, mouseGrabY, mouseGrabShape);
}

void mouse_pointer_draw_now(SDL_Surface *copy)
{
	if (!has_mouse || mouseInactive)
		return;

	const MousePointerSpriteInfo *spriteInfo = &mousePointerSprites[mouseCursor];
	const Sint32 x = mouseX - spriteInfo->x - spriteInfo->fx;
	const Sint32 y = mouseY - spriteInfo->y - spriteInfo->fy;
	if (pointerFilter >= 0)
		blit_sprite2x2_filter_clip(copy, x, y, shopSpriteSheet, spriteInfo->index, pointerFilter);
	else
		blit_sprite2x2_clip(copy, x, y, shopSpriteSheet, spriteInfo->index);
}

Uint32 mouse_pointer_between(void)
{
	if (!pointerShown || !has_mouse || mouseInactive)
		return UINT32_MAX;
	const Uint32 wait = smooth_picture_wait();
	if (wait == UINT32_MAX || wait > 0)
		return wait;
	if (mouseX == pointerX && mouseY == pointerY)
		return MAX(2, smooth_refresh_ms() / 4);  // look again soon, without waking up all the time

	if (pointerScreen == NULL || pointerScreen->w != VGAScreen->w || pointerScreen->h != VGAScreen->h)
	{
		SDL_FreeSurface(pointerScreen);
		pointerScreen = SDL_CreateRGBSurface(0, VGAScreen->w, VGAScreen->h, 8, 0, 0, 0, 0);
		if (pointerScreen == NULL)
			return UINT32_MAX;
	}

	// the screen is as it was shown, without the pointer
	memcpy(pointerScreen->pixels, VGAScreen->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);
	mouse_pointer_draw_now(pointerScreen);
	video_present(pointerScreen);

	smooth_picture_shown();

	pointerX = mouseX;
	pointerY = mouseY;
	return 1;
}
