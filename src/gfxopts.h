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
#ifndef GFXOPTS_H
#define GFXOPTS_H

#include "opentyr.h"

// The video options of Setup > Graphics and of the pause menu's Graphics
// page.  Both screens show and change them through these functions, so an
// option is written once.  Each option has a list of values (numbered from
// 0) and the current one is always one of them.

typedef enum
{
	GFX_OPT_NONE = -1,
	GFX_OPT_DISPLAY,         // window, then each display
	GFX_OPT_SCALER,
	GFX_OPT_SCALING_MODE,
	GFX_OPT_SHARP_BILINEAR,  // off, on
	GFX_OPT_VSYNC,           // off, on
	GFX_OPT_SMOOTH_MOTION,   // off, on
	GFX_OPT_BRIGHTNESS,      // gammaCorrection
	GFX_OPT_COUNT
} GfxOption;

size_t gfx_option_count(GfxOption option);
size_t gfx_option_current(GfxOption option);

// Name of value i, translated; buffer is used only for "Display N".
const char *gfx_option_value_name(GfxOption option, size_t i, char *buffer, size_t bufferSize);

// Name of the current value.
const char *gfx_option_current_name(GfxOption option, char *buffer, size_t bufferSize);

// Changes to value i, like choosing it in the menu (a scaler or Smooth
// Motion picked this way counts as chosen).  The brightness is the one
// used when a level starts; the pause menu applies it to the level itself.
void gfx_option_set(GfxOption option, size_t i);

// Changes to the next (step 1) or previous (step -1) value, wrapping around.
void gfx_option_step(GfxOption option, int step);

#endif /* GFXOPTS_H */
