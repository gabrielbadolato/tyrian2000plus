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
#ifndef UPDATE_CHECK_H
#define UPDATE_CHECK_H

#include <stdbool.h>
#include <stddef.h>

// "check" in the "updates" section of opentyrian.cfg; on by default
extern bool update_check_enabled;

// asks GitHub, in the background, for the latest published release; does
// nothing if the check is off, already done or this build has no version
void update_check_start(void);

// the newer release's tag (e.g. "v1.0.3"), or NULL if there is none, the
// answer has not arrived yet, or the check is off
const char *update_check_newer_version(void);

// a published release and its notes (Markdown, as written on GitHub)
typedef struct
{
	char tag[32];   // "v1.3"
	char date[11];  // "2026-10-20", or ""
	char *notes;
} ReleaseNotes;

#define UPDATE_CHECK_MAX_RELEASES 10

// the newest releases with their notes, as GitHub last gave them (kept in
// release-notes.txt, so also without internet); newest first, maybe none
const ReleaseNotes *update_check_releases(size_t *count);

// "v1.2.3" (or "v1.2", anything after it ignored) as numbers; false if it is
// not a version
bool update_check_parse_version(const char *text, int version[3]);
int update_check_compare_versions(const int a[3], const int b[3]);

// for the tests: GitHub's list of releases (drafts and pre-releases left
// out) and the release-notes.txt file; they return how many were read
size_t update_check_parse_releases(const char *json, ReleaseNotes *list, size_t max);
size_t update_check_parse_notes_file(const char *text, ReleaseNotes *list, size_t max);

// where the newer release can be downloaded
#define UPDATE_CHECK_ITCH_URL "https://gabrielbadolato.itch.io/tyrian2000plus"
#define UPDATE_CHECK_RELEASES_URL "https://github.com/gabrielbadolato/tyrian2000plus/releases/latest"

#endif /* UPDATE_CHECK_H */
