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
#include "gfxopts.h"

#include "config.h"
#include "lang.h"
#include "smooth.h"
#include "video.h"
#include "video_scale.h"

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>

static const char *const brightnessNames[] = { "Normal", "Bright", "Brighter", "Brightest" };

size_t gfx_option_count(GfxOption option)
{
	switch (option)
	{
	case GFX_OPT_DISPLAY:
		return 1 + (size_t)SDL_GetNumVideoDisplays();
	case GFX_OPT_SCALER:
		return scalers_count;
	case GFX_OPT_SCALING_MODE:
		return ScalingMode_MAX;
	case GFX_OPT_SHARP_BILINEAR:
	case GFX_OPT_VSYNC:
	case GFX_OPT_SMOOTH_MOTION:
		return 2;
	case GFX_OPT_BRIGHTNESS:
		return COUNTOF(brightnessNames);
	default:
		return 0;
	}
}

size_t gfx_option_current(GfxOption option)
{
	switch (option)
	{
	case GFX_OPT_DISPLAY:
		return (size_t)(fullscreen_display + 1);
	case GFX_OPT_SCALER:
		return scaler;
	case GFX_OPT_SCALING_MODE:
		return scaling_mode;
	case GFX_OPT_SHARP_BILINEAR:
		return sharp_bilinear_enabled ? 1 : 0;
	case GFX_OPT_VSYNC:
		return vsync_enabled ? 1 : 0;
	case GFX_OPT_SMOOTH_MOTION:
		return smooth_motion ? 1 : 0;
	case GFX_OPT_BRIGHTNESS:
		return gammaCorrection % COUNTOF(brightnessNames);
	default:
		return 0;
	}
}

const char *gfx_option_value_name(GfxOption option, size_t i, char *buffer, size_t bufferSize)
{
	switch (option)
	{
	case GFX_OPT_DISPLAY:
		if (i == 0)
			return tr("Window");
		snprintf(buffer, bufferSize, tr("Display %d"), (int)i);
		return buffer;
	case GFX_OPT_SCALER:
		return i < scalers_count ? scalers[i].name : "";
	case GFX_OPT_SCALING_MODE:
		return i < ScalingMode_MAX ? tr(scaling_mode_names[i]) : "";
	case GFX_OPT_SHARP_BILINEAR:
	case GFX_OPT_VSYNC:
	case GFX_OPT_SMOOTH_MOTION:
		return i == 1 ? tr("On") : tr("Off");
	case GFX_OPT_BRIGHTNESS:
		return i < COUNTOF(brightnessNames) ? tr(brightnessNames[i]) : "";
	default:
		return "";
	}
}

const char *gfx_option_current_name(GfxOption option, char *buffer, size_t bufferSize)
{
	return gfx_option_value_name(option, gfx_option_current(option), buffer, bufferSize);
}

void gfx_option_set(GfxOption option, size_t i)
{
	if (i >= gfx_option_count(option))
		return;

	switch (option)
	{
	case GFX_OPT_DISPLAY:
		if ((int)i - 1 != fullscreen_display)
			reinit_fullscreen((int)i - 1);
		break;
	case GFX_OPT_SCALER:
		if (i != scaler)
		{
			scaler_chosen = true;
			const uint oldScaler = scaler;
			if (!init_scaler((uint)i) &&     // try new scaler
			    !init_scaler(oldScaler))     // revert on fail
			{
				exit(EXIT_FAILURE);
			}
		}
		break;
	case GFX_OPT_SCALING_MODE:
		scaling_mode = (ScalingMode)i;
		break;
	case GFX_OPT_SHARP_BILINEAR:
		set_sharp_bilinear(i == 1);
		break;
	case GFX_OPT_VSYNC:
		set_vsync(i == 1);
		break;
	case GFX_OPT_SMOOTH_MOTION:
		smooth_motion = i == 1;
		smooth_motion_chosen = true;
		break;
	case GFX_OPT_BRIGHTNESS:
		gammaCorrection = (JE_byte)i;
		break;
	default:
		break;
	}
}

void gfx_option_step(GfxOption option, int step)
{
	const size_t count = gfx_option_count(option);
	if (count == 0)
		return;
	const int n = (int)count;
	gfx_option_set(option, (size_t)(((int)gfx_option_current(option) + n + step % n) % n));
}
