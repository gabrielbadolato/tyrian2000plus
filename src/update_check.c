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
 * latest published release of Tyrian 2000+ and compares its tag with this
 * build's version.  Nothing about the player is sent, only an ordinary HTTPS
 * request.  Without internet, or on any error, nothing is shown.
 *
 * Windows uses WinHTTP (part of Windows); elsewhere curl is run, so the
 * static Linux build needs no TLS library of its own.
 */
#include "update_check.h"

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
#define API_PATH "/repos/gabrielbadolato/tyrian2000plus/releases/latest"

bool update_check_enabled = true;

static bool started = false;
static SDL_atomic_t answered;  // set once newerVersion is filled in
static char newerVersion[32];

// reads "v1.2" or "v1.2.3" (anything after that, like "-5-gabc123", is
// ignored) into major, minor, patch; false if it is not a version
static bool parseVersion(const char *text, int version[3])
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

static int compareVersions(const int a[3], const int b[3])
{
	for (int i = 0; i < 3; ++i)
	{
		if (a[i] != b[i])
			return a[i] < b[i] ? -1 : 1;
	}
	return 0;
}

// fills buffer with the start of the API's answer; false on any failure
#ifdef _WIN32
static bool fetchLatestRelease(char *buffer, size_t bufferSize)
{
	bool ok = false;
	size_t length = 0;

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

	while (length + 1 < bufferSize)
	{
		DWORD read = 0;
		if (!WinHttpReadData(request, buffer + length, (DWORD)(bufferSize - 1 - length), &read))
			goto done;
		if (read == 0)
			break;
		length += read;
	}

	ok = length > 0;

done:
	buffer[length] = '\0';

	if (request != NULL)
		WinHttpCloseHandle(request);
	if (connection != NULL)
		WinHttpCloseHandle(connection);
	if (session != NULL)
		WinHttpCloseHandle(session);

	return ok;
}
#else
static bool fetchLatestRelease(char *buffer, size_t bufferSize)
{
	// LD_PRELOAD is dropped so the Steam overlay is not loaded into curl
	FILE *pipe = popen("env -u LD_PRELOAD curl -fsSL --max-time 10 -A Tyrian2000Plus "
	                   "-H 'Accept: application/vnd.github+json' "
	                   "'https://" API_HOST API_PATH "' 2>/dev/null", "r");
	if (pipe == NULL)
		return false;

	size_t length = 0;
	while (length + 1 < bufferSize)
	{
		const size_t read = fread(buffer + length, 1, bufferSize - 1 - length, pipe);
		if (read == 0)
			break;
		length += read;
	}
	buffer[length] = '\0';

	// read the rest, so curl is not cut off before it exits
	char discard[1024];
	while (fread(discard, 1, sizeof(discard), pipe) > 0)
		;

	return pclose(pipe) == 0 && length > 0;
}
#endif

// copies the value of "tag_name" in the JSON answer; false if it is missing
static bool findTagName(const char *json, char *tag, size_t tagSize)
{
	const char *p = strstr(json, "\"tag_name\"");
	if (p == NULL)
		return false;
	p += strlen("\"tag_name\"");

	while (isspace((unsigned char)*p))
		++p;
	if (*p++ != ':')
		return false;
	while (isspace((unsigned char)*p))
		++p;
	if (*p++ != '"')
		return false;

	size_t length = 0;
	while (*p != '"')
	{
		if (!(isalnum((unsigned char)*p) || *p == '.' || *p == '-' || *p == '_') || length + 1 >= tagSize)
			return false;
		tag[length++] = *p++;
	}
	tag[length] = '\0';

	return length > 0;
}

static int SDLCALL updateCheckThread(void *data)
{
	const int *currentVersion = data;

	static char json[16384];
	char tag[sizeof(newerVersion)];
	int latestVersion[3];

	if (!fetchLatestRelease(json, sizeof(json)) || !findTagName(json, tag, sizeof(tag)))
	{
		logInfo("Update check: could not reach GitHub.");
		return 0;
	}

	if (!parseVersion(tag, latestVersion))
	{
		logInfo("Update check: latest release \"%s\" is not a version.", tag);
		return 0;
	}

	if (compareVersions(latestVersion, currentVersion) > 0)
	{
		logInfo("Update check: %s is available (this is %s).", tag, opentyrian_version);

		SDL_strlcpy(newerVersion, tag, sizeof(newerVersion));
		SDL_AtomicSet(&answered, 1);
	}
	else
	{
		logInfo("Update check: %s is the latest release.", tag);
	}

	return 0;
}

void update_check_start(void)
{
	static int currentVersion[3];

	if (!update_check_enabled || started)
		return;
	started = true;

	// builds from a plain commit (no release tag) have nothing to compare
	if (!parseVersion(opentyrian_version, currentVersion))
	{
		logInfo("Update check: skipped, \"%s\" is not a release version.", opentyrian_version);
		return;
	}

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
