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

/*
 * The FM music chip.  Both emulators get every register write, so the music
 * keeps playing when the setting changes; only the chosen one makes sound.
 * The music player runs in the audio thread (or with the device locked).
 */
#include "oplchip.h"

#include "loudness.h"
#include "opl.h"
#include "opl3.h"

#include <stdint.h>

bool opl_accurate = false;
bool opl_chosen = false;

static opl3_chip nuked;

void opl_set_accurate(bool accurate)
{
	opl_accurate = accurate;  // read by the audio thread at its next block
}

void opl_init(void)
{
	adlib_init(audioSampleRate);
	OPL3_Reset(&nuked, audioSampleRate);
}

void opl_write(Bitu reg, Bit8u val)
{
	adlib_write(reg, val);
	OPL3_WriteReg(&nuked, (Bit16u)reg, val);
}

void opl_update(Bit16s *buf, Bits num)
{
	if (!opl_accurate)
	{
		adlib_getsample(buf, num);
		return;
	}

	// The music is OPL2 (mono): Nuked sends it to both sides, keep one.
	// Nuked is a little quieter than the DOSBox emulator (measured over all
	// the songs), so raise it to the same loudness.
	for (Bits i = 0; i < num; ++i)
	{
		Bit16s stereo[2];
		OPL3_GenerateResampled(&nuked, stereo);
		const int sample = stereo[0] * 6 / 5;
		buf[i] = (Bit16s)(sample > INT16_MAX ? INT16_MAX : sample < INT16_MIN ? INT16_MIN : sample);
	}
}
