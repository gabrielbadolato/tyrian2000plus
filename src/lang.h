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
#ifndef LANG_H
#define LANG_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Translations of the game's texts.  The game data stays unchanged: every
 * text is looked up by its English original when it is loaded (tyrian.hdt)
 * or drawn (the program's own menus), and the translation, if there is one,
 * is used instead.  Each language is a table of English/translated pairs in
 * its own file (lang_pt_br.c), written in UTF-8 and converted to the game's
 * character set (CP437, plus the letters accents.h adds).
 */

typedef struct
{
	const char *english;
	const char *translation;
}
LangEntry;

// 0 = automatic (the system's language), 1 = English, 2 and up = translations;
// "language" in the "language" section of opentyrian.cfg
extern int lang_setting;

// while true, readEncryptedString() translates what it reads (only around
// the texts of tyrian.hdt, so the data cubes and credits are left alone)
extern bool lang_translate_reads;

size_t lang_setting_count(void);
const char *lang_setting_name(size_t i);        // as shown in Setup
const char *lang_setting_code(int setting);     // as saved: "auto", "en", "pt-br"
int lang_setting_from_code(const char *code);   // -1 if unknown

// picks the language for the setting; call lang_reload_texts() afterwards
// when the game texts were already loaded
void lang_set(int setting);

// true when a translation is in use
bool lang_active(void);

// the translation of an English text, or the text itself
const char *tr(const char *english);

// replaces the English text in buffer by its translation (cut to size)
void lang_translate(char *buffer, size_t size);

// loads the game's own texts again, in the current language
void lang_reload_texts(void);

// converts UTF-8 to the game's character set (unknown letters become '?')
void lang_from_utf8(char *dst, size_t size, const char *utf8);

#endif /* LANG_H */
