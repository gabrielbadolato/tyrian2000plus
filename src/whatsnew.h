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
#ifndef WHATSNEW_H
#define WHATSNEW_H

#include <stdbool.h>

/*
 * What's New: the notes of each version, shown by whatsNewScreen() (about.c).
 * They come from the releases on GitHub (in English and Portuguese, see
 * update_check.c) and, for what GitHub has not given, from the CHANGELOG.md
 * built into the program (in English).
 */

// the version whose notes the player has already seen ("notes_seen" in the
// "updates" section of opentyrian.cfg); "" when there was no opentyrian.cfg
extern char whatsnew_seen[32];

// this version (e.g. "v1.4") when the player updated to it and has not
// opened What's New yet, or NULL
const char *whatsnew_notice_version(void);

// What's New was opened: the notice goes away
void whatsnew_mark_seen(void);

typedef enum
{
	NOTES_BLANK,
	NOTES_HEADING,     // a version
	NOTES_SUBHEADING,  // a heading inside the notes
	NOTES_TEXT,
	NOTES_BULLET,      // an item of a list
	NOTES_SUBBULLET,   // an item of a list inside an item
	NOTES_LINK,
} NotesStyle;

// receives the screen's lines, already in the game's character set (url
// only for NOTES_LINK); the text may need to be broken into lines
typedef void (*NotesSink)(NotesStyle style, const char *text, const char *url);

// the notes of every version, newest first, in the current language when
// there are notes in it
void whatsnew_build(NotesSink sink);

// for the tests: one version's notes (UTF-8 Markdown), in Portuguese if
// portuguese and they have a part in it, else in English
void whatsnew_build_notes(const char *markdown, bool portuguese, NotesSink sink);

#endif /* WHATSNEW_H */
