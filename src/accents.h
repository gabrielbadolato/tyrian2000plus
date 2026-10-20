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
#ifndef ACCENTS_H
#define ACCENTS_H

/*
 * Accented letters for the translations.  The game's character set is CP437:
 * its accented letters (128-168) are drawn only in the small font, so the
 * others get them here, built from the plain letter and an accent mark.
 * Letters CP437 lacks (Portuguese ã, õ and most accented capitals) get the
 * character codes below.  Accented capitals keep the accent only in the
 * small font; the larger fonts have no room above their capitals and draw
 * the plain letter (Ç keeps its cedilla).
 */
typedef enum
{
	ACCENT_FIRST = 0xF0,

	ACCENT_A_TILDE_SMALL = ACCENT_FIRST,  // ã
	ACCENT_O_TILDE_SMALL,                 // õ
	ACCENT_A_ACUTE,                       // Á
	ACCENT_A_GRAVE,                       // À
	ACCENT_A_CIRCUMFLEX,                  // Â
	ACCENT_A_TILDE,                       // Ã
	ACCENT_E_CIRCUMFLEX,                  // Ê
	ACCENT_I_ACUTE,                       // Í
	ACCENT_O_ACUTE,                       // Ó
	ACCENT_O_CIRCUMFLEX,                  // Ô
	ACCENT_O_TILDE,                       // Õ
	ACCENT_U_ACUTE,                       // Ú

	ACCENT_END
}
AccentChar;

// font sprite index of the first of these letters, the same in every font table
#define ACCENT_SPRITE_FIRST 176

// adds the accented letters to the three font tables; call after loading the fonts
void accents_install(void);

#endif /* ACCENTS_H */
