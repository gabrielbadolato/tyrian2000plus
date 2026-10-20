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
#ifndef PADGLYPH_H
#define PADGLYPH_H

#include <stdbool.h>

/*
 * Controller button icons that can be written inside any text: each one is a
 * character code above the font's own (CP437 169 and up are unused), drawn by
 * every text routine like a letter, in the text's color.
 */
typedef enum
{
	PADGLYPH_FIRST = 0xB0,

	// same on every controller
	PADGLYPH_DPAD_UP = PADGLYPH_FIRST,
	PADGLYPH_DPAD_RIGHT,
	PADGLYPH_DPAD_DOWN,
	PADGLYPH_DPAD_LEFT,
	PADGLYPH_ARROW_UP,     // after a stick: which way it is pushed
	PADGLYPH_ARROW_RIGHT,
	PADGLYPH_ARROW_DOWN,
	PADGLYPH_ARROW_LEFT,
	PADGLYPH_STICK_LEFT,   // stick seen from above
	PADGLYPH_STICK_RIGHT,

	// Xbox style (also Steam Controller, Steam Deck and other XInput pads)
	PADGLYPH_XBOX_A,
	PADGLYPH_XBOX_B,
	PADGLYPH_XBOX_X,
	PADGLYPH_XBOX_Y,
	PADGLYPH_XBOX_VIEW,
	PADGLYPH_XBOX_GUIDE,
	PADGLYPH_XBOX_MENU,
	PADGLYPH_XBOX_LS,
	PADGLYPH_XBOX_RS,
	PADGLYPH_XBOX_LB,
	PADGLYPH_XBOX_RB,
	PADGLYPH_XBOX_LT,
	PADGLYPH_XBOX_RT,

	// PlayStation style
	PADGLYPH_PS_CROSS,
	PADGLYPH_PS_CIRCLE,
	PADGLYPH_PS_SQUARE,
	PADGLYPH_PS_TRIANGLE,
	PADGLYPH_PS_CREATE,
	PADGLYPH_PS_HOME,
	PADGLYPH_PS_OPTIONS,
	PADGLYPH_PS_L3,
	PADGLYPH_PS_R3,
	PADGLYPH_PS_L1,
	PADGLYPH_PS_R1,
	PADGLYPH_PS_L2,
	PADGLYPH_PS_R2,

	PADGLYPH_END
}
PadGlyph;

// font sprite index of the first icon, the same in every font table
#define PADGLYPH_SPRITE_FIRST 128

static inline bool is_padglyph(unsigned char ch)
{
	return ch >= PADGLYPH_FIRST && ch < PADGLYPH_END;
}

// draws the icons into the three font tables; call after loading the fonts
void padglyphs_install(void);

#endif /* PADGLYPH_H */
