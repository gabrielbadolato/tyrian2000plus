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
 * What's New: the notes of each version.  A release's notes on GitHub have
 * an English part and a Portuguese one (under "**English**" and
 * "**Português**"); the screen shows the one of the current language.  The
 * versions GitHub has not given (no internet yet, or older than the ten it
 * sends) come from CHANGELOG.md, which the Makefile builds into the program.
 * The notes are Markdown; only what they use is understood: lists, bold,
 * code, links and headings.
 */
#include "whatsnew.h"

#include "lang.h"
#include "opentyr.h"
#include "update_check.h"

#include "SDL.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const changelogLines[] =
{
#ifdef EMBEDDED_CHANGELOG
#include "changelog.inc"
#endif
	NULL
};

char whatsnew_seen[32] = "";

// this build's version without what follows it ("v1.3-5-gabc" -> "v1.3"),
// false if it has none
static bool currentVersion(char *tag, size_t size, int version[3])
{
	if (!update_check_parse_version(opentyrian_version, version))
		return false;

	size_t length = strcspn(opentyrian_version, "-+ ");
	SDL_strlcpy(tag, opentyrian_version, MIN(length + 1, size));
	return true;
}

const char *whatsnew_notice_version(void)
{
	static char tag[32];
	int current[3];
	if (!currentVersion(tag, sizeof(tag), current))
		return NULL;

	// no opentyrian.cfg: a new player, nothing to tell
	if (whatsnew_seen[0] == '\0')
		SDL_strlcpy(whatsnew_seen, tag, sizeof(whatsnew_seen));

	int seen[3];
	if (update_check_parse_version(whatsnew_seen, seen) && update_check_compare_versions(seen, current) >= 0)
		return NULL;

	return tag;
}

void whatsnew_mark_seen(void)
{
	char tag[32];
	int current[3];
	if (currentVersion(tag, sizeof(tag), current))
		SDL_strlcpy(whatsnew_seen, tag, sizeof(whatsnew_seen));
}

/*** Markdown ***/

// removes what the screen cannot show: **bold**, `code`, [text](link)
// becomes text, and " (#41)" (the pull requests, for GitHub readers)
static void cleanMarkdown(char *text)
{
	char *out = text;
	for (const char *in = text; *in != '\0'; )
	{
		if ((in[0] == '*' && in[1] == '*') || (in[0] == '_' && in[1] == '_'))
		{
			in += 2;
		}
		else if (*in == '`')
		{
			++in;
		}
		else if (*in == '[' && strchr(in, ']') != NULL && strchr(in, ']')[1] == '(' && strchr(strchr(in, ']'), ')') != NULL)
		{
			// [text](link): the text only
			const char *close = strchr(in, ']');
			memmove(out, in + 1, (size_t)(close - in - 1));
			out += close - in - 1;
			in = strchr(close, ')') + 1;
		}
		else if (*in == '(' && in[1] == '#' && isdigit((unsigned char)in[2]))
		{
			// (#41) or (#41, #42)
			const char *end = in + 1;
			while (*end == '#' || *end == ',' || *end == ' ' || isdigit((unsigned char)*end))
				++end;
			if (*end == ')')
			{
				while (out > text && out[-1] == ' ')
					--out;
				in = end + 1;
			}
			else
			{
				*out++ = *in++;
			}
		}
		else
		{
			*out++ = *in++;
		}
	}
	*out = '\0';
}

static void emit(NotesSink sink, NotesStyle style, const char *utf8, const char *url)
{
	// the small font has no '>' ("Setup > Sound" becomes "Setup / Sound")
	char slashes[2048];
	SDL_strlcpy(slashes, utf8, sizeof(slashes));
	for (char *c = slashes; (c = strstr(c, " > ")) != NULL; c += 3)
		c[1] = '/';

	char text[2048];
	lang_from_utf8(text, sizeof(text), slashes);
	sink(style, text, url);
}

// a paragraph or a list item, once all its lines are together
static void emitParagraph(NotesSink sink, NotesStyle style, char *text)
{
	cleanMarkdown(text);

	// trim
	char *start = text;
	while (*start == ' ')
		++start;
	size_t length = strlen(start);
	while (length > 0 && start[length - 1] == ' ')
		start[--length] = '\0';
	if (*start == '\0')
		return;

	// "Full list: https://..." becomes a link named "Full list"
	char *url = strstr(start, "https://");
	if (url == NULL)
		url = strstr(start, "http://");
	if (url != NULL && style != NOTES_SUBHEADING)
	{
		char link[256];
		SDL_strlcpy(link, url, MIN(strcspn(url, " )") + 1, sizeof(link)));

		char *label = start;
		*url = '\0';
		length = strlen(label);
		while (length > 0 && (label[length - 1] == ' ' || label[length - 1] == ':' || label[length - 1] == '('))
			label[--length] = '\0';
		if (*label == '\0')
			label = strstr(link, "://") + 3;

		emit(sink, NOTES_LINK, label, link);
		return;
	}

	emit(sink, style, start, NULL);
}

// the language of a line like "**English**" or "## Português", or -1
static int languageHeading(const char *line, size_t length)
{
	char word[32];
	size_t n = 0;
	for (size_t i = 0; i < length && n + 1 < sizeof(word); ++i)
		if (strchr("*#_: \t\r", line[i]) == NULL)
			word[n++] = line[i];
	word[n] = '\0';

	if (SDL_strcasecmp(word, "English") == 0)
		return 0;
	if (SDL_strcasecmp(word, "Português") == 0 || SDL_strcasecmp(word, "Portugues") == 0 || SDL_strcasecmp(word, "Portuguese") == 0)
		return 1;
	return -1;
}

void whatsnew_build_notes(const char *markdown, bool portuguese, NotesSink sink)
{
	// Does it have a part for each language?  What comes before the first
	// language heading is English (the draft of a release has only English,
	// and the Portuguese part is added below it).
	bool hasLanguages = false;
	bool hasWanted = false;  // with something in it
	int language = 0;
	for (const char *line = markdown; *line != '\0'; )
	{
		const size_t length = strcspn(line, "\n");
		const int heading = languageHeading(line, length);
		if (heading >= 0)
		{
			hasLanguages = true;
			language = heading;
		}
		else if (language == (portuguese ? 1 : 0) && strspn(line, " \t\r") < length)
		{
			hasWanted = true;
		}
		line += length + (line[length] != '\0' ? 1 : 0);
	}
	const int wanted = !hasLanguages ? -1 : hasWanted ? (portuguese ? 1 : 0) : 0;

	language = 0;  // of the part being read
	char paragraph[2048] = "";
	NotesStyle style = NOTES_TEXT;
	bool any = false;    // something was shown
	bool blank = false;  // a blank line is due before what comes next

#define FLUSH() do { if (paragraph[0] != '\0') { emitParagraph(sink, style, paragraph); paragraph[0] = '\0'; any = true; } } while (0)
#define BLANK() do { if (blank) sink(NOTES_BLANK, "", NULL); blank = false; } while (0)

	for (const char *line = markdown; *line != '\0'; )
	{
		size_t length = strcspn(line, "\n");
		const char *next = line + length + (line[length] != '\0' ? 1 : 0);
		while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == ' ' || line[length - 1] == '\t'))
			--length;

		size_t indent = 0;
		while (indent < length && (line[indent] == ' ' || line[indent] == '\t'))
			++indent;
		const char *text = line + indent;
		const size_t textLength = length - indent;

		const int heading = languageHeading(line, length);
		const bool listItem = textLength >= 2 && (text[0] == '-' || text[0] == '*' || text[0] == '+') && text[1] == ' ';
		const bool title = textLength > 0 && text[0] == '#';
		const bool rule = textLength >= 3 && strspn(text, "-*_ ") >= textLength;

		line = next;

		if (heading >= 0)
		{
			FLUSH();
			language = heading;
			continue;
		}
		if (wanted >= 0 && language != wanted)
			continue;  // the other language's part

		if (textLength == 0 || rule)
		{
			FLUSH();
			blank = any;
		}
		else if (title)
		{
			FLUSH();
			BLANK();
			size_t skip = strspn(text, "# ");
			char buffer[256];
			SDL_strlcpy(buffer, text + skip, MIN(textLength - skip + 1, sizeof(buffer)));
			emitParagraph(sink, NOTES_SUBHEADING, buffer);
			any = true;
		}
		else if (listItem)
		{
			FLUSH();
			BLANK();
			style = indent >= 2 ? NOTES_SUBBULLET : NOTES_BULLET;
			SDL_strlcpy(paragraph, text + 2, MIN(textLength - 2 + 1, sizeof(paragraph)));
		}
		else if (paragraph[0] == '\0')
		{
			BLANK();
			style = NOTES_TEXT;
			SDL_strlcpy(paragraph, text, MIN(textLength + 1, sizeof(paragraph)));
		}
		else
		{
			// the next line of the same paragraph or item
			SDL_strlcat(paragraph, " ", sizeof(paragraph));
			const size_t at = strlen(paragraph);
			SDL_strlcpy(paragraph + at, text, MIN(textLength + 1, sizeof(paragraph) - at));
		}
	}
	FLUSH();

#undef FLUSH
#undef BLANK
}

/*** the versions ***/

typedef struct
{
	char tag[32];        // "" for "Next version"
	int version[3];
	const char *notes;   // from GitHub, if it has them
	char *changelog;     // from CHANGELOG.md (allocated), if it has them
} Entry;

#define MAX_ENTRIES 64

static int sortCurrent[3];  // this build's version, for compareEntries

static int compareEntries(const void *a, const void *b)
{
	const Entry *x = a, *y = b;

	// "Next version" goes above this build's version, below newer releases
	if ((x->tag[0] == '\0') != (y->tag[0] == '\0'))
	{
		const Entry *release = x->tag[0] == '\0' ? y : x;
		const bool releaseFirst = update_check_compare_versions(release->version, sortCurrent) > 0;
		return (release == x) == releaseFirst ? -1 : 1;
	}
	return -update_check_compare_versions(x->version, y->version);
}

// CHANGELOG.md's "## v1.3 (2026-10-20)" sections (and "## Next version")
static size_t changelogEntries(Entry *entries, size_t max)
{
	size_t count = 0;
	Entry *entry = NULL;
	size_t size = 0;

	for (size_t i = 0; changelogLines[i] != NULL; ++i)
	{
		const char *line = changelogLines[i];

		if (strncmp(line, "## ", 3) == 0)
		{
			entry = NULL;
			int version[3] = { 0, 0, 0 };
			const bool next = SDL_strncasecmp(line + 3, "Next version", 12) == 0;
			if (count < max && (next || update_check_parse_version(line + 3, version)))
			{
				entry = &entries[count++];
				memset(entry, 0, sizeof(*entry));
				if (!next)
					SDL_strlcpy(entry->tag, line + 3, MIN(strcspn(line + 3, " ") + 1, sizeof(entry->tag)));
				memcpy(entry->version, version, sizeof(version));
				size = 0;
			}
			continue;
		}

		if (entry == NULL)
			continue;

		const size_t length = strlen(line);
		char *grown = realloc(entry->changelog, size + length + 2);
		if (grown == NULL)
			continue;
		entry->changelog = grown;
		memcpy(entry->changelog + size, line, length);
		size += length;
		entry->changelog[size++] = '\n';
		entry->changelog[size] = '\0';
	}

	// "Next version" with nothing in it yet (a release build has none)
	for (size_t i = 0; i < count; )
	{
		const char *text = entries[i].changelog != NULL ? entries[i].changelog : "";
		if (entries[i].tag[0] == '\0' && strspn(text, " \n") == strlen(text))
		{
			free(entries[i].changelog);
			entries[i] = entries[--count];
		}
		else
		{
			++i;
		}
	}

	return count;
}

void whatsnew_build(NotesSink sink)
{
	const bool portuguese = lang_active();

	char currentTag[32];
	int current[3];
	const bool hasCurrent = currentVersion(currentTag, sizeof(currentTag), current);

	static Entry entries[MAX_ENTRIES];
	size_t count = changelogEntries(entries, MAX_ENTRIES);

	size_t releasesCount;
	const ReleaseNotes *releases = update_check_releases(&releasesCount);
	for (size_t r = 0; r < releasesCount; ++r)
	{
		int version[3];
		if (!update_check_parse_version(releases[r].tag, version))
			continue;

		Entry *entry = NULL;
		for (size_t i = 0; i < count && entry == NULL; ++i)
			if (entries[i].tag[0] != '\0' && update_check_compare_versions(entries[i].version, version) == 0)
				entry = &entries[i];

		if (entry == NULL && count < MAX_ENTRIES)
		{
			entry = &entries[count++];
			memset(entry, 0, sizeof(*entry));
			SDL_strlcpy(entry->tag, releases[r].tag, sizeof(entry->tag));
			memcpy(entry->version, version, sizeof(version));
		}
		if (entry != NULL && releases[r].notes[0] != '\0')
			entry->notes = releases[r].notes;
	}

	if (hasCurrent)
		memcpy(sortCurrent, current, sizeof(sortCurrent));
	else
		memset(sortCurrent, 0, sizeof(sortCurrent));
	qsort(entries, count, sizeof(*entries), compareEntries);

	if (count == 0)
		sink(NOTES_TEXT, tr("No notes yet. They come with each new version."), NULL);

	bool englishSaid = false;
	for (size_t i = 0; i < count; ++i)
	{
		const Entry *entry = &entries[i];

		if (portuguese && entry->notes == NULL && !englishSaid)
		{
			sink(NOTES_TEXT, tr("The notes below are in English only."), NULL);
			sink(NOTES_BLANK, "", NULL);
			englishSaid = true;
		}

		char heading[96];
		if (entry->tag[0] == '\0')
			snprintf(heading, sizeof(heading), "%s", tr("Coming in the next version"));
		else if (hasCurrent && update_check_compare_versions(entry->version, current) > 0)
			snprintf(heading, sizeof(heading), tr("%s - new"), entry->tag);  // the normal font has no "()"
		else if (hasCurrent && update_check_compare_versions(entry->version, current) == 0)
			snprintf(heading, sizeof(heading), tr("%s - this version"), entry->tag);
		else
			snprintf(heading, sizeof(heading), "%s", entry->tag);
		sink(NOTES_HEADING, heading, NULL);

		whatsnew_build_notes(entry->notes != NULL ? entry->notes : entry->changelog != NULL ? entry->changelog : "", portuguese, sink);
		sink(NOTES_BLANK, "", NULL);
	}

	for (size_t i = 0; i < count; ++i)
	{
		free(entries[i].changelog);
		entries[i].changelog = NULL;
	}
}
