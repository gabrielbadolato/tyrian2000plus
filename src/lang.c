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
#include "lang.h"

#include "accents.h"
#include "episodes.h"
#include "helptext.h"
#include "logging.h"
#include "opentyr.h"

#include "SDL.h"

#include <stdlib.h>
#include <string.h>

extern const LangEntry lang_pt_br[];

typedef struct
{
	const char *code;     // as saved in opentyrian.cfg
	const char *locale;   // system language that picks it automatically
	const char *name;     // as shown in Setup, in UTF-8
	const LangEntry *entries;
}
Language;

static const Language languages[] =
{
	{ "pt-br", "pt", "Português", lang_pt_br },
};

int lang_setting = 0;
bool lang_translate_reads = false;

// the current language's pairs, in the game's character set, sorted by English text
typedef struct
{
	char *english;
	char *translation;
}
Pair;

static Pair *pairs = NULL;
static size_t pairsCount = 0;

static char settingNames[2 + COUNTOF(languages)][32];

size_t lang_setting_count(void)
{
	return 2 + COUNTOF(languages);
}

const char *lang_setting_name(size_t i)
{
	if (i == 0)
		return tr("Automatic");
	if (i == 1)
		return "English";
	if (i - 2 < COUNTOF(languages))
	{
		lang_from_utf8(settingNames[i], sizeof(settingNames[i]), languages[i - 2].name);
		return settingNames[i];
	}
	return "";
}

const char *lang_setting_code(int setting)
{
	if (setting == 1)
		return "en";
	if (setting >= 2 && (size_t)(setting - 2) < COUNTOF(languages))
		return languages[setting - 2].code;
	return "auto";
}

int lang_setting_from_code(const char *code)
{
	for (int i = 0; (size_t)i < lang_setting_count(); ++i)
		if (SDL_strcasecmp(code, lang_setting_code(i)) == 0)
			return i;
	return -1;
}

// the system's first preferred language we have, or -1 for English
static int systemLanguage(void)
{
	int found = -1;

	SDL_Locale *locales = SDL_GetPreferredLocales();
	if (locales == NULL)
		return -1;

	for (const SDL_Locale *l = locales; l->language != NULL && found < 0; ++l)
	{
		if (SDL_strcasecmp(l->language, "en") == 0)
			break;
		for (size_t i = 0; i < COUNTOF(languages); ++i)
		{
			if (SDL_strcasecmp(l->language, languages[i].locale) == 0)
			{
				found = (int)i;
				break;
			}
		}
	}

	SDL_free(locales);
	return found;
}

void lang_from_utf8(char *dst, size_t size, const char *utf8)
{
	// Unicode code points of CP437 128-168
	static const Uint16 cp437[] =
	{
		0xC7, 0xFC, 0xE9, 0xE2, 0xE4, 0xE0, 0xE5, 0xE7, 0xEA, 0xEB, 0xE8, 0xEF, 0xEE, 0xEC, 0xC4, 0xC5,
		0xC9, 0xE6, 0xC6, 0xF4, 0xF6, 0xF2, 0xFB, 0xF9, 0xFF, 0xD6, 0xDC, 0xA2, 0xA3, 0xA5, 0x20A7, 0x192,
		0xE1, 0xED, 0xF3, 0xFA, 0xF1, 0xD1, 0xAA, 0xBA, 0xBF,
	};
	static const struct { Uint16 code; Uint8 ch; } extra[] =
	{
		{ 0xE3, ACCENT_A_TILDE_SMALL },
		{ 0xF5, ACCENT_O_TILDE_SMALL },
		{ 0xC1, ACCENT_A_ACUTE },
		{ 0xC0, ACCENT_A_GRAVE },
		{ 0xC2, ACCENT_A_CIRCUMFLEX },
		{ 0xC3, ACCENT_A_TILDE },
		{ 0xCA, ACCENT_E_CIRCUMFLEX },
		{ 0xCD, ACCENT_I_ACUTE },
		{ 0xD3, ACCENT_O_ACUTE },
		{ 0xD4, ACCENT_O_CIRCUMFLEX },
		{ 0xD5, ACCENT_O_TILDE },
		{ 0xDA, ACCENT_U_ACUTE },
		{ 0x25A0, 254 },  // ■, a separator in the help texts
	};

	if (size == 0)
		return;

	size_t n = 0;
	const unsigned char *s = (const unsigned char *)utf8;
	while (*s != '\0' && n + 1 < size)
	{
		Uint32 code = *s++;
		if (code >= 0x80)
		{
			int more = code >= 0xF0 ? 3 : code >= 0xE0 ? 2 : code >= 0xC0 ? 1 : 0;
			code &= 0x3F >> more;
			for (; more > 0 && (*s & 0xC0) == 0x80; --more)
				code = (code << 6) | (*s++ & 0x3F);
		}

		char ch = '?';
		if (code < 0x80)
			ch = (char)code;
		for (size_t i = 0; i < COUNTOF(cp437); ++i)
			if (code >= 0x80 && cp437[i] == code)
				ch = (char)(128 + i);
		for (size_t i = 0; i < COUNTOF(extra); ++i)
			if (extra[i].code == code)
				ch = (char)extra[i].ch;

		dst[n++] = ch;
	}
	dst[n] = '\0';
}

static char *convert(const char *utf8)
{
	const size_t size = strlen(utf8) + 1;  // never grows: one byte per letter
	char *s = malloc(size);
	if (s != NULL)
		lang_from_utf8(s, size, utf8);
	return s;
}

static int comparePairs(const void *a, const void *b)
{
	return strcmp(((const Pair *)a)->english, ((const Pair *)b)->english);
}

static void freePairs(void)
{
	for (size_t i = 0; i < pairsCount; ++i)
	{
		free(pairs[i].english);
		free(pairs[i].translation);
	}
	free(pairs);
	pairs = NULL;
	pairsCount = 0;
}

void lang_set(int setting)
{
	if (setting < 0 || (size_t)setting >= lang_setting_count())
		setting = 0;
	lang_setting = setting;

	const int language = setting == 0 ? systemLanguage() : setting - 2;

	freePairs();

	if (language < 0)
	{
		logInfo("Language: English");
		return;
	}

	const LangEntry *entries = languages[language].entries;

	size_t count = 0;
	while (entries[count].english != NULL)
		++count;

	pairs = calloc(count, sizeof(*pairs));
	if (pairs == NULL)
		return;

	for (size_t i = 0; i < count; ++i)
	{
		pairs[pairsCount].english = convert(entries[i].english);
		pairs[pairsCount].translation = convert(entries[i].translation);
		if (pairs[pairsCount].english == NULL || pairs[pairsCount].translation == NULL)
		{
			free(pairs[pairsCount].english);
			free(pairs[pairsCount].translation);
			continue;
		}
		++pairsCount;
	}

	qsort(pairs, pairsCount, sizeof(*pairs), comparePairs);

	logInfo("Language: %s (%u texts)", languages[language].code, (unsigned int)pairsCount);
}

bool lang_active(void)
{
	return pairsCount > 0;
}

const char *tr(const char *english)
{
	if (pairsCount == 0 || english == NULL || english[0] == '\0')
		return english;

	// texts in tyrian.hdt may be padded with spaces
	size_t len = strlen(english);
	while (len > 0 && english[len - 1] == ' ')
		--len;

	char key[256];
	const char *lookup = english;
	if (english[len] != '\0')
	{
		if (len >= sizeof(key))
			return english;
		memcpy(key, english, len);
		key[len] = '\0';
		lookup = key;
	}

	const Pair wanted = { (char *)lookup, NULL };
	const Pair *found = bsearch(&wanted, pairs, pairsCount, sizeof(*pairs), comparePairs);

	return found != NULL ? found->translation : english;
}

void lang_translate(char *buffer, size_t size)
{
	const char *translation = tr(buffer);
	if (translation != buffer)
		SDL_strlcpy(buffer, translation, size);
}

void lang_reload_texts(void)
{
	JE_loadHelpText();
	JE_loadItemDat();
}
