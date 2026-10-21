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
/* Headless test for What's New: GitHub's list of releases (src/update_check.c),
 * release-notes.txt, the Markdown of the notes in English and Portuguese and
 * the CHANGELOG.md built into the program (src/whatsnew.c), and when the
 * title screen tells about the notes.
 *
 * Usage: test_whatsnew [releases.json]  (a saved answer of the GitHub API,
 * read too if given) */
#include "lang.h"
#include "opentyr.h"
#include "update_check.h"
#include "whatsnew.h"

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;

static void check(bool ok, const char *what)
{
	printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
	fails += !ok;
}

/*** what the screen would get ***/

static char got[512][2048];
static char gotUrl[512][256];
static NotesStyle gotStyle[512];
static int gotCount;

static void sink(NotesStyle style, const char *text, const char *url)
{
	if (gotCount >= 512)
		return;
	gotStyle[gotCount] = style;
	snprintf(got[gotCount], sizeof(got[gotCount]), "%s", text);
	snprintf(gotUrl[gotCount], sizeof(gotUrl[gotCount]), "%s", url != NULL ? url : "");
	++gotCount;
}

static int find(NotesStyle style, const char *text)
{
	for (int i = 0; i < gotCount; ++i)
		if (gotStyle[i] == style && strcmp(got[i], text) == 0)
			return i;
	return -1;
}

static bool anyContains(const char *text)
{
	for (int i = 0; i < gotCount; ++i)
		if (strstr(got[i], text) != NULL)
			return true;
	return false;
}

static void print(void)
{
	static const char *const names[] = { "blank", "HEAD", "sub", "text", "-", "  -", "link" };
	for (int i = 0; i < gotCount; ++i)
		printf("      %-5s %s%s%s\n", names[gotStyle[i]], got[i], gotUrl[i][0] != '\0' ? " -> " : "", gotUrl[i]);
}

static const char json[] =
	"[\n"
	"  {\"url\": \"x\", \"tag_name\": \"v1.5-rc1\", \"prerelease\": true, \"draft\": false, \"body\": \"skip\"},\n"
	"  {\"tag_name\": \"v1.6\", \"draft\": true, \"body\": \"a draft\"},\n"
	"  {\"assets\": [{\"name\": \"a.zip\", \"size\": 12, \"uploader\": {\"login\": \"x\", \"site_admin\": false}}],\n"
	"   \"tag_name\": \"v1.4\", \"published_at\": \"2026-11-02T12:00:00Z\", \"prerelease\": false,\n"
	"   \"body\": \"**English**\\r\\n\\r\\n- **New:** a \\\"thing\\\" (#50)\\r\\n- Two\\r\\n  lines\\r\\n\\r\\n"
	"Full list: https://github.com/gabrielbadolato/tyrian2000plus/blob/v1.4/CHANGELOG.md\\r\\n\\r\\n"
	"**Portugu\\u00eas**\\r\\n\\r\\n- **Novo:** uma coisa \\u2013 com a\\u00e7\\u00e3o \\ud83d\\ude00\\r\\n\\r\\n"
	"Lista completa (em ingl\\u00eas): https://github.com/gabrielbadolato/tyrian2000plus/blob/v1.4/CHANGELOG.md\"},\n"
	"  {\"tag_name\": \"v1.3\", \"body\": null, \"reactions\": {\"+1\": 2}, \"n\": [1, 2.5e3, -1, {}, []]},\n"
	"  {\"tag_name\": \"not-a-version\", \"body\": \"x\"}\n"
	"]\n";

int main(int argc, char *argv[])
{
	ReleaseNotes list[UPDATE_CHECK_MAX_RELEASES];

	printf("GitHub's answer\n");
	size_t count = update_check_parse_releases(json, list, UPDATE_CHECK_MAX_RELEASES);
	check(count == 2, "two releases (no draft, pre-release or odd tag)");
	check(count >= 1 && strcmp(list[0].tag, "v1.4") == 0, "the first is v1.4");
	check(count >= 1 && strcmp(list[0].date, "2026-11-02") == 0, "with its date");
	check(count >= 1 && strstr(list[0].notes, "a \"thing\"") != NULL, "escaped quotes");
	check(count >= 1 && strstr(list[0].notes, "Portugu\xc3\xaas") != NULL, "\\u00ea as UTF-8");
	check(count >= 1 && strstr(list[0].notes, "\xf0\x9f\x98\x80") != NULL, "a surrogate pair as UTF-8");
	check(count >= 2 && strcmp(list[1].tag, "v1.3") == 0 && list[1].notes[0] == '\0', "v1.3, with no notes (null)");
	check(update_check_parse_releases("[", list + 2, 1) == 0, "a cut answer gives nothing");
	check(update_check_parse_releases("{\"message\": \"rate limit\"}", list + 2, 1) == 0, "an error message gives nothing");
	check(update_check_parse_releases("[]", list + 2, 1) == 0, "an empty list gives nothing");
	check(update_check_parse_releases(json, list + 2, 1) == 1, "stops at the size given");
	free(list[2].notes);

	printf("release-notes.txt\n");
	{
		ReleaseNotes kept[4];
		const char *file = "# comment\n@@ v1.4 2026-11-02\nline one\r\n @@ not a header\n\n@@ v1.3 \n@@ bad\nlost\n@@ v1.2.1 2026-10-01\nlast";
		const size_t n = update_check_parse_notes_file(file, kept, 4);
		check(n == 3, "three versions (\"bad\" is not one)");
		check(n >= 1 && strcmp(kept[0].tag, "v1.4") == 0 && strcmp(kept[0].date, "2026-11-02") == 0, "tag and date");
		check(n >= 1 && strcmp(kept[0].notes, "line one\n @@ not a header\n\n") == 0, "the notes, lines kept");
		check(n >= 2 && strcmp(kept[1].tag, "v1.3") == 0 && kept[1].date[0] == '\0', "no date");
		check(n >= 3 && strcmp(kept[2].tag, "v1.2.1") == 0 && strcmp(kept[2].notes, "last\n") == 0, "the last one");
		for (size_t i = 0; i < n; ++i)
			free(kept[i].notes);
	}

	printf("notes in English\n");
	gotCount = 0;
	whatsnew_build_notes(list[0].notes, false, sink);
	print();
	check(find(NOTES_BULLET, "New: a \"thing\"") >= 0, "an item, no bold, no (#50)");
	check(find(NOTES_BULLET, "Two lines") >= 0, "an item on two lines");
	{
		const int i = find(NOTES_LINK, "Full list");
		check(i >= 0 && strcmp(gotUrl[i], "https://github.com/gabrielbadolato/tyrian2000plus/blob/v1.4/CHANGELOG.md") == 0, "the link, named by its text");
	}
	check(!anyContains("Novo"), "nothing of the Portuguese part");
	check(gotCount > 0 && gotStyle[0] != NOTES_BLANK, "no blank line first");

	printf("notes in Portuguese\n");
	gotCount = 0;
	whatsnew_build_notes(list[0].notes, true, sink);
	print();
	{
		char expected[128];
		lang_from_utf8(expected, sizeof(expected), "Novo: uma coisa - com a\xc3\xa7\xc3\xa3o ?");
		check(find(NOTES_BULLET, expected) >= 0, "accents in the game's letters, a dash, no emoji");
		lang_from_utf8(expected, sizeof(expected), "Lista completa (em ingl\xc3\xaas)");
		check(find(NOTES_LINK, expected) >= 0, "the Portuguese link");
	}
	check(!anyContains("thing"), "nothing of the English part");

	printf("notes with no language parts\n");
	gotCount = 0;
	whatsnew_build_notes("Intro text\nsecond line\n\n### Fixes\n* one\n---\n2. [Docs](https://x.org/a) are `here`.", true, sink);
	print();
	check(find(NOTES_TEXT, "Intro text second line") >= 0, "a paragraph");
	check(find(NOTES_SUBHEADING, "Fixes") >= 0, "a heading");
	check(find(NOTES_BULLET, "one") >= 0, "a * item");
	check(find(NOTES_TEXT, "2. Docs are here.") >= 0, "a link's text only, no code marks");

	printf("only English, asked for Portuguese\n");
	gotCount = 0;
	whatsnew_build_notes("**English**\n- only\n", true, sink);
	check(find(NOTES_BULLET, "only") >= 0, "the English part");
	gotCount = 0;
	whatsnew_build_notes("**English**\n- only\n\n**Portugu\xc3\xaas**\n\n", true, sink);
	check(find(NOTES_BULLET, "only") >= 0 && gotCount == 1, "an empty Portuguese part: the English one");

	printf("English with no heading, then Portuguese\n");
	gotCount = 0;
	whatsnew_build_notes("- one\n- two\n\n**Portugu\xc3\xaas**\n- um\n", false, sink);
	check(find(NOTES_BULLET, "one") >= 0 && find(NOTES_BULLET, "two") >= 0 && find(NOTES_BULLET, "um") < 0, "English: what comes first");
	gotCount = 0;
	whatsnew_build_notes("- one\n- two\n\n**Portugu\xc3\xaas**\n- um\n", true, sink);
	check(find(NOTES_BULLET, "um") >= 0 && gotCount == 1, "Portuguese: only its part");

	printf("the versions\n");
	opentyrian_version = "v1.2-3-gabcdef";
	gotCount = 0;
	whatsnew_build(sink);
	print();
	check(gotCount > 0, "something");
	check(find(NOTES_HEADING, "v1.2 - this version") >= 0, "this version is marked");
	check(find(NOTES_HEADING, "v1.0") >= 0, "the first version is there");
	check(anyContains("Setup / Sound / \"Music Sound\""), "no '>', which the font lacks");
	{
		// CHANGELOG.md has this section only between releases
		const int next = find(NOTES_HEADING, "Coming in the next version");
		check(next < 0 || (next > find(NOTES_HEADING, "v1.3 - new") && next < find(NOTES_HEADING, "v1.2 - this version")), "the next version between newer releases and this one");
	}
	check(find(NOTES_HEADING, "v1.3 - new") >= 0 && find(NOTES_HEADING, "v1.3 - new") < find(NOTES_HEADING, "v1.2 - this version"), "newest first, newer ones marked");
	check(find(NOTES_SUBBULLET, "Fixes:") < 0 && find(NOTES_BULLET, "Fixes:") >= 0, "an item...");
	check(find(NOTES_SUBBULLET, "Sharp Bilinear: a one-off failure no longer turns the saved option off.") >= 0, "...and the items inside it");

	printf("the notice on the title screen\n");
	opentyrian_version = "v1.4";
	whatsnew_seen[0] = '\0';
	check(whatsnew_notice_version() == NULL, "a new player: no notice");
	check(strcmp(whatsnew_seen, "v1.4") == 0, "and the version counts as seen");
	SDL_strlcpy(whatsnew_seen, "v0", sizeof(whatsnew_seen));
	check(whatsnew_notice_version() != NULL && strcmp(whatsnew_notice_version(), "v1.4") == 0, "updated from before What's New: notice");
	SDL_strlcpy(whatsnew_seen, "v1.3", sizeof(whatsnew_seen));
	check(whatsnew_notice_version() != NULL, "updated from v1.3: notice");
	whatsnew_mark_seen();
	check(whatsnew_notice_version() == NULL && strcmp(whatsnew_seen, "v1.4") == 0, "seen: no notice");
	SDL_strlcpy(whatsnew_seen, "v1.5", sizeof(whatsnew_seen));
	check(whatsnew_notice_version() == NULL, "went back to an older version: no notice");
	opentyrian_version = "abc1234";
	SDL_strlcpy(whatsnew_seen, "v0", sizeof(whatsnew_seen));
	check(whatsnew_notice_version() == NULL, "a build with no version: no notice");

	if (argc > 1)
	{
		printf("%s\n", argv[1]);
		FILE *f = fopen(argv[1], "rb");
		static char real[4 * 1024 * 1024];
		const size_t n = f != NULL ? fread(real, 1, sizeof(real) - 1, f) : 0;
		real[n] = '\0';
		if (f != NULL)
			fclose(f);

		ReleaseNotes realList[UPDATE_CHECK_MAX_RELEASES];
		const size_t realCount = update_check_parse_releases(real, realList, UPDATE_CHECK_MAX_RELEASES);
		check(realCount > 0, "releases read");
		for (size_t i = 0; i < realCount; ++i)
		{
			gotCount = 0;
			whatsnew_build_notes(realList[i].notes, true, sink);
			printf("    %s (%s): %d lines in Portuguese\n", realList[i].tag, realList[i].date, gotCount);
			if (i == 0)
				print();
			free(realList[i].notes);
		}
	}

	for (size_t i = 0; i < count; ++i)
		free(list[i].notes);

	printf(fails == 0 ? "all ok\n" : "%d FAILED\n", fails);
	return fails == 0 ? 0 : 1;
}
