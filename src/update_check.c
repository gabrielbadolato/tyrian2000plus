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
 * Update check: at startup a background thread asks the GitHub API for the
 * latest published releases of Tyrian 2000+ and compares their tags with this
 * build's version.  Nothing about the player is sent, only an ordinary HTTPS
 * request.  Without internet, or on any error, nothing is shown.
 *
 * The same answer brings each release's notes, for the What's New screen
 * (whatsnew.c).  They are kept in release-notes.txt in the user folder, so
 * the screen still has them without internet or with the check turned off.
 *
 * Windows uses WinHTTP (part of Windows); elsewhere curl is run, so the
 * static Linux build needs no TLS library of its own.
 */
#include "update_check.h"

#include "file.h"
#include "logging.h"
#include "opentyr.h"

#include "SDL.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#ifdef _MSC_VER
#pragma comment(lib, "winhttp.lib")
#endif
#endif

#define API_HOST "api.github.com"
#define API_PATH "/repos/gabrielbadolato/tyrian2000plus/releases?per_page=10"  // UPDATE_CHECK_MAX_RELEASES

#define NOTES_FILENAME "release-notes.txt"

bool update_check_enabled = true;

static bool started = false;
static SDL_atomic_t answered;  // set once newerVersion is filled in
static char newerVersion[32];

// the releases the screen shows (from GitHub, or kept from the last time)
static ReleaseNotes releases[UPDATE_CHECK_MAX_RELEASES];
static size_t releasesCount;
static bool releasesLoaded;

// filled in by the thread; taken by update_check_releases() once ready is set
static ReleaseNotes fetched[UPDATE_CHECK_MAX_RELEASES];
static size_t fetchedCount;
static SDL_atomic_t fetchedReady;

// reads "v1.2" or "v1.2.3" (anything after that, like "-5-gabc123", is
// ignored) into major, minor, patch; false if it is not a version
bool update_check_parse_version(const char *text, int version[3])
{
	if (*text == 'v' || *text == 'V')
		++text;

	for (int i = 0; i < 3; ++i)
	{
		version[i] = 0;

		if (i > 0)
		{
			if (*text != '.')
				return i >= 2;
			++text;
		}

		if (!isdigit((unsigned char)*text))
			return false;

		while (isdigit((unsigned char)*text))
		{
			version[i] = version[i] * 10 + (*text - '0');
			if (version[i] > 9999)
				return false;
			++text;
		}
	}

	return true;
}

int update_check_compare_versions(const int a[3], const int b[3])
{
	for (int i = 0; i < 3; ++i)
	{
		if (a[i] != b[i])
			return a[i] < b[i] ? -1 : 1;
	}
	return 0;
}

static void freeReleases(ReleaseNotes *list, size_t *count)
{
	for (size_t i = 0; i < *count; ++i)
	{
		free(list[i].notes);
		list[i].notes = NULL;
	}
	*count = 0;
}

// the API's answer, in a buffer that grows as needed (NULL on any failure);
// the caller frees it
#define MAX_ANSWER_SIZE (2 * 1024 * 1024)

static bool appendAnswer(char **buffer, size_t *length, size_t *capacity, const char *data, size_t size)
{
	if (*length + size + 1 > *capacity)
	{
		size_t newCapacity = *capacity > 0 ? *capacity : 64 * 1024;
		while (*length + size + 1 > newCapacity)
			newCapacity *= 2;
		if (newCapacity > MAX_ANSWER_SIZE)
			return false;

		char *newBuffer = realloc(*buffer, newCapacity);
		if (newBuffer == NULL)
			return false;
		*buffer = newBuffer;
		*capacity = newCapacity;
	}

	memcpy(*buffer + *length, data, size);
	*length += size;
	(*buffer)[*length] = '\0';
	return true;
}

#ifdef _WIN32
static char *fetchReleases(void)
{
	bool ok = false;
	char *buffer = NULL;
	size_t length = 0;
	size_t capacity = 0;

	HINTERNET session = WinHttpOpen(L"Tyrian2000Plus", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	HINTERNET connection = NULL;
	HINTERNET request = NULL;

	if (session == NULL)
		goto done;

	WinHttpSetTimeouts(session, 5000, 5000, 5000, 5000);

	connection = WinHttpConnect(session, L"" API_HOST, INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (connection == NULL)
		goto done;

	request = WinHttpOpenRequest(connection, L"GET", L"" API_PATH, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (request == NULL)
		goto done;

	if (!WinHttpSendRequest(request, L"Accept: application/vnd.github+json\r\n", (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
	    !WinHttpReceiveResponse(request, NULL))
		goto done;

	DWORD status = 0;
	DWORD statusSize = sizeof(status);
	if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &statusSize, WINHTTP_NO_HEADER_INDEX) ||
	    status != 200)
		goto done;

	for (; ; )
	{
		char data[16384];
		DWORD read = 0;
		if (!WinHttpReadData(request, data, sizeof(data), &read))
			goto done;
		if (read == 0)
			break;
		if (!appendAnswer(&buffer, &length, &capacity, data, read))
			goto done;
	}

	ok = length > 0;

done:
	if (request != NULL)
		WinHttpCloseHandle(request);
	if (connection != NULL)
		WinHttpCloseHandle(connection);
	if (session != NULL)
		WinHttpCloseHandle(session);

	if (!ok)
	{
		free(buffer);
		return NULL;
	}
	return buffer;
}
#elif defined(__EMSCRIPTEN__)
// the browser version never checks (see update_check_start)
static char *fetchReleases(void)
{
	return NULL;
}
#else
static char *fetchReleases(void)
{
	// LD_PRELOAD is dropped so the Steam overlay is not loaded into curl
	FILE *pipe = popen("env -u LD_PRELOAD curl -fsSL --max-time 10 -A Tyrian2000Plus "
	                   "-H 'Accept: application/vnd.github+json' "
	                   "'https://" API_HOST API_PATH "' 2>/dev/null", "r");
	if (pipe == NULL)
		return NULL;

	char *buffer = NULL;
	size_t length = 0;
	size_t capacity = 0;
	bool ok = true;

	char data[16384];
	size_t read;
	while ((read = fread(data, 1, sizeof(data), pipe)) > 0)
	{
		// after a failure, read the rest anyway, so curl is not cut off before it exits
		if (ok)
			ok = appendAnswer(&buffer, &length, &capacity, data, read);
	}

	if (pclose(pipe) != 0 || !ok || length == 0)
	{
		free(buffer);
		return NULL;
	}
	return buffer;
}
#endif

/*** a small JSON reader, just enough for the API's list of releases ***/

static void skipSpace(const char **p)
{
	while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')
		++*p;
}

static int hexDigit(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static bool readHex4(const char *p, unsigned *value)
{
	*value = 0;
	for (int i = 0; i < 4; ++i)
	{
		const int digit = hexDigit(p[i]);
		if (digit < 0)
			return false;
		*value = *value * 16 + (unsigned)digit;
	}
	return true;
}

// reads a JSON string at *p (which points at its opening quote) as UTF-8
// into out (cut to outSize, which may be 0 to skip it); returns the full
// length it needs, or -1 if it is not a valid string
static long readString(const char **p, char *out, size_t outSize)
{
	if (**p != '"')
		return -1;
	const char *s = *p + 1;

	size_t length = 0;
#define PUT(c) do { if (length + 1 < outSize) out[length] = (char)(c); ++length; } while (0)

	for (; ; )
	{
		const unsigned char c = (unsigned char)*s++;
		if (c == '\0')
			return -1;
		if (c == '"')
			break;
		if (c != '\\')
		{
			PUT(c);
			continue;
		}

		const char e = *s++;
		switch (e)
		{
		case '"': PUT('"'); break;
		case '\\': PUT('\\'); break;
		case '/': PUT('/'); break;
		case 'b': PUT('\b'); break;
		case 'f': PUT('\f'); break;
		case 'n': PUT('\n'); break;
		case 'r': PUT('\r'); break;
		case 't': PUT('\t'); break;
		case 'u':
		{
			unsigned code;
			if (!readHex4(s, &code))
				return -1;
			s += 4;

			// a pair of surrogates is one character
			if (code >= 0xD800 && code < 0xDC00 && s[0] == '\\' && s[1] == 'u')
			{
				unsigned low;
				if (readHex4(s + 2, &low) && low >= 0xDC00 && low < 0xE000)
				{
					code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
					s += 6;
				}
			}

			if (code < 0x80)
			{
				PUT(code);
			}
			else if (code < 0x800)
			{
				PUT(0xC0 | (code >> 6));
				PUT(0x80 | (code & 0x3F));
			}
			else if (code < 0x10000)
			{
				PUT(0xE0 | (code >> 12));
				PUT(0x80 | ((code >> 6) & 0x3F));
				PUT(0x80 | (code & 0x3F));
			}
			else
			{
				PUT(0xF0 | (code >> 18));
				PUT(0x80 | ((code >> 12) & 0x3F));
				PUT(0x80 | ((code >> 6) & 0x3F));
				PUT(0x80 | (code & 0x3F));
			}
			break;
		}
		default:
			return -1;
		}
	}
#undef PUT

	if (outSize > 0)
		out[MIN(length, outSize - 1)] = '\0';

	*p = s;
	return (long)length;
}

static bool skipValue(const char **p, int depth)
{
	if (depth > 32)
		return false;

	skipSpace(p);
	const char c = **p;

	if (c == '"')
		return readString(p, NULL, 0) >= 0;

	if (c == '{' || c == '[')
	{
		const char close = c == '{' ? '}' : ']';
		++*p;
		skipSpace(p);
		if (**p == close)
		{
			++*p;
			return true;
		}
		for (; ; )
		{
			if (c == '{')
			{
				skipSpace(p);
				if (readString(p, NULL, 0) < 0)
					return false;
				skipSpace(p);
				if (**p != ':')
					return false;
				++*p;
			}
			if (!skipValue(p, depth + 1))
				return false;
			skipSpace(p);
			if (**p == ',')
			{
				++*p;
				continue;
			}
			if (**p != close)
				return false;
			++*p;
			return true;
		}
	}

	// a number, true, false or null
	const char *start = *p;
	while (**p != '\0' && strchr(",}] \t\r\n", **p) == NULL)
		++*p;
	return *p > start;
}

// a JSON string's value, allocated (NULL if it is not a string, e.g. null)
static char *readStringAlloc(const char **p)
{
	const char *s = *p;
	const long length = readString(&s, NULL, 0);
	if (length < 0)
		return NULL;

	char *text = malloc((size_t)length + 1);
	if (text == NULL)
		return NULL;
	readString(p, text, (size_t)length + 1);
	return text;
}

size_t update_check_parse_releases(const char *json, ReleaseNotes *list, size_t max)
{
	size_t count = 0;
	const char *p = json;

	skipSpace(&p);
	if (*p != '[')
		return 0;
	++p;
	skipSpace(&p);
	if (*p == ']')
		return 0;

	for (; ; )
	{
		skipSpace(&p);
		if (*p != '{')
			break;
		++p;

		ReleaseNotes release = { "", "", NULL };
		bool skip = false;  // a draft or a pre-release
		bool ok = true;

		skipSpace(&p);
		if (*p == '}')
		{
			++p;
		}
		else for (; ; )
		{
			char key[32];
			skipSpace(&p);
			if (readString(&p, key, sizeof(key)) < 0)
			{
				ok = false;
				break;
			}
			skipSpace(&p);
			if (*p != ':')
			{
				ok = false;
				break;
			}
			++p;
			skipSpace(&p);

			if (strcmp(key, "tag_name") == 0 && *p == '"')
			{
				readString(&p, release.tag, sizeof(release.tag));
			}
			else if (strcmp(key, "published_at") == 0 && *p == '"')
			{
				readString(&p, release.date, sizeof(release.date));
			}
			else if (strcmp(key, "body") == 0 && *p == '"')
			{
				free(release.notes);
				release.notes = readStringAlloc(&p);
			}
			else if ((strcmp(key, "draft") == 0 || strcmp(key, "prerelease") == 0) && strncmp(p, "true", 4) == 0)
			{
				skip = true;
				p += 4;
			}
			else if (!skipValue(&p, 1))
			{
				ok = false;
				break;
			}

			skipSpace(&p);
			if (*p == ',')
			{
				++p;
				continue;
			}
			if (*p == '}')
			{
				++p;
				break;
			}
			ok = false;
			break;
		}

		int version[3];
		if (ok && !skip && count < max && update_check_parse_version(release.tag, version))
		{
			if (release.notes == NULL)
				release.notes = calloc(1, 1);
			if (release.notes != NULL)
			{
				list[count++] = release;
				release.notes = NULL;
			}
		}
		free(release.notes);

		if (!ok)
			break;

		skipSpace(&p);
		if (*p != ',')
			break;
		++p;
	}

	return count;
}

/*** release-notes.txt: "@@ tag date" lines, each followed by its notes ***/

static void saveNotes(const ReleaseNotes *list, size_t count)
{
	File file = userFileOpen(NOTES_FILENAME, "wb");
	if (file.error)
		return;

	fputs("# Release notes of Tyrian 2000+, from GitHub, for the What's New screen.\n", file.f);
	for (size_t i = 0; i < count; ++i)
	{
		fprintf(file.f, "@@ %s %s\n", list[i].tag, list[i].date);

		// one line at a time, so a line of the notes can never pass for "@@"
		for (const char *line = list[i].notes; *line != '\0'; )
		{
			const char *end = strchr(line, '\n');
			size_t length = end != NULL ? (size_t)(end - line) : strlen(line);
			const char *next = line + length + (end != NULL ? 1 : 0);
			if (length > 0 && line[length - 1] == '\r')
				--length;
			if (length >= 2 && line[0] == '@' && line[1] == '@')
				fputc(' ', file.f);
			fwrite(line, 1, length, file.f);
			fputc('\n', file.f);
			line = next;
		}
	}

	if (ferror(file.f))
		logWarn("Update check: could not write '%s'.", NOTES_FILENAME);
	fileClose(&file);
}

size_t update_check_parse_notes_file(const char *text, ReleaseNotes *list, size_t max)
{
	size_t count = 0;
	ReleaseNotes *current = NULL;
	size_t notesLength = 0;

	for (const char *line = text; *line != '\0'; )
	{
		const char *end = strchr(line, '\n');
		size_t length = end != NULL ? (size_t)(end - line) : strlen(line);
		const char *next = line + length + (end != NULL ? 1 : 0);
		if (length > 0 && line[length - 1] == '\r')
			--length;

		if (length >= 3 && strncmp(line, "@@ ", 3) == 0)
		{
			current = NULL;
			if (count < max)
			{
				ReleaseNotes *release = &list[count];
				char header[64];
				SDL_strlcpy(header, line + 3, MIN(length - 3 + 1, sizeof(header)));
				char *space = strchr(header, ' ');
				if (space != NULL)
					*space = '\0';
				SDL_strlcpy(release->tag, header, sizeof(release->tag));
				SDL_strlcpy(release->date, space != NULL ? space + 1 : "", sizeof(release->date));

				int version[3];
				release->notes = calloc(1, (size_t)(strlen(next) + 1));
				if (release->notes != NULL && update_check_parse_version(release->tag, version))
				{
					current = release;
					notesLength = 0;
					++count;
				}
				else
				{
					free(release->notes);
					release->notes = NULL;
				}
			}
		}
		else if (current != NULL)
		{
			memcpy(current->notes + notesLength, line, length);
			notesLength += length;
			current->notes[notesLength++] = '\n';
			current->notes[notesLength] = '\0';
		}

		line = next;
	}

	return count;
}

static void loadNotes(void)
{
	File file = userFileOpen(NOTES_FILENAME, "rb");
	if (file.error)
		return;

	const long length = fileGetLength(&file);
	char *text = length > 0 && length < MAX_ANSWER_SIZE ? malloc((size_t)length + 1) : NULL;
	if (text != NULL)
	{
		const size_t read = fileReadAtMost(&file, text, (size_t)length);
		text[read] = '\0';
		releasesCount = update_check_parse_notes_file(text, releases, COUNTOF(releases));
		free(text);
	}
	fileClose(&file);
}

const ReleaseNotes *update_check_releases(size_t *count)
{
	if (!releasesLoaded)
	{
		releasesLoaded = true;
		loadNotes();
	}

	// the thread is done with fetched[] once it sets fetchedReady
	if (SDL_AtomicGet(&fetchedReady) == 1)
	{
		freeReleases(releases, &releasesCount);
		memcpy(releases, fetched, sizeof(releases));
		releasesCount = fetchedCount;
		fetchedCount = 0;
		SDL_AtomicSet(&fetchedReady, 2);
	}

	*count = releasesCount;
	return releases;
}

static int SDLCALL updateCheckThread(void *data)
{
	const int *currentVersion = data;

	char *json = fetchReleases();
	if (json == NULL)
	{
		logInfo("Update check: could not reach GitHub.");
		return 0;
	}

	fetchedCount = update_check_parse_releases(json, fetched, COUNTOF(fetched));
	free(json);

	if (fetchedCount == 0)
	{
		logInfo("Update check: no release found in GitHub's answer.");
		return 0;
	}

	saveNotes(fetched, fetchedCount);

	// the newest release, whatever the order of the answer
	const char *tag = NULL;
	int latestVersion[3] = { 0, 0, 0 };
	for (size_t i = 0; i < fetchedCount; ++i)
	{
		int version[3];
		if (update_check_parse_version(fetched[i].tag, version) &&
		    (tag == NULL || update_check_compare_versions(version, latestVersion) > 0))
		{
			tag = fetched[i].tag;
			memcpy(latestVersion, version, sizeof(latestVersion));
		}
	}

	if (update_check_compare_versions(latestVersion, currentVersion) > 0)
	{
		logInfo("Update check: %s is available (this is %s).", tag, opentyrian_version);

		SDL_strlcpy(newerVersion, tag, sizeof(newerVersion));
		SDL_AtomicSet(&answered, 1);
	}
	else
	{
		logInfo("Update check: %s is the latest release.", tag);
	}

	SDL_AtomicSet(&fetchedReady, 1);

	return 0;
}

void update_check_start(void)
{
	static int currentVersion[3];

	if (!update_check_enabled || started)
		return;
	started = true;

#ifdef __EMSCRIPTEN__
	// The browser version is always the newest one, and has no threads.
	return;
#endif

	// builds from a plain commit (no release tag) have nothing to compare
	if (!update_check_parse_version(opentyrian_version, currentVersion))
	{
		logInfo("Update check: skipped, \"%s\" is not a release version.", opentyrian_version);
		return;
	}

	// what was kept from the last time, before the thread brings news
	size_t count;
	update_check_releases(&count);

	SDL_Thread *thread = SDL_CreateThread(updateCheckThread, "update check", currentVersion);
	if (thread == NULL)
	{
		logWarn("Update check: could not start: %s", SDL_GetError());
		return;
	}
	SDL_DetachThread(thread);
}

const char *update_check_newer_version(void)
{
	if (!update_check_enabled || SDL_AtomicGet(&answered) == 0)
		return NULL;

	return newerVersion;
}
