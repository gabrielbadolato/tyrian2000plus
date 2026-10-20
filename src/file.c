/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) The OpenTyrian Development Team
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
#include "file.h"

#include "opentyr.h"

#include "SDL.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

const char *customDataDirPath = NULL;

enum
{
	ERRNUM_EOF = -1,
};

static const char *dataDirPath = NULL;
static size_t dataDirPathLen = 0;

static char *userDirPath = NULL;
static size_t userDirPathLen = 0;

static bool fileExists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f != NULL)
		fclose(f);
	return f != NULL;
}

static File fileOpen(const char *path, const char *mode)
{
	errno = 0;  // fopen might not set errno
	FILE *f = fopen(path, mode);
	return (File) { f, errno, f == NULL };
}

bool findDataFiles(void)
{
	dataDirPath = NULL;
	dataDirPathLen = 0;

	const char *filename = "tyrian1.lvl";

	if (customDataDirPath != NULL)
	{
		dataDirPath = customDataDirPath;
		dataDirPathLen = strlen(dataDirPath);

		return dataFileExists(filename);
	}

	// A "data" directory next to the executable (or inside the app bundle's
	// Resources on macOS), so a self-contained distribution runs from any cwd.
	static char *baseDataDirPath = NULL;
	if (baseDataDirPath == NULL)
	{
		char *basePath = SDL_GetBasePath();
		if (basePath != NULL)
		{
			size_t baseDataDirPathSize = strlen(basePath) + strlen("data") + 1;
			baseDataDirPath = malloc(baseDataDirPathSize);
			snprintf(baseDataDirPath, baseDataDirPathSize, "%sdata", basePath);
			SDL_free(basePath);
		}
	}

	const char *dataDirPaths[] =
	{
		baseDataDirPath,
#ifdef TYRIAN_DIR
		TYRIAN_DIR,
#endif
	};

	for (size_t i = 0; i < COUNTOF(dataDirPaths); ++i)
	{
		if (dataDirPaths[i] == NULL)
			continue;

		dataDirPath = dataDirPaths[i];
		dataDirPathLen = strlen(dataDirPath);

		if (dataFileExists(filename))
			return true;
	}

	dataDirPath = "";
	dataDirPathLen = 0;

	return fileExists(filename);
}

bool dataFileExists(const char *filename)
{
	File file = dataFileOpen(filename, "rb");

	bool result = !file.error;

	fileClose(&file);

	return result;
}

bool userFileExists(const char *filename)
{
	File file = userFileOpen(filename, "rb");

	bool result = !file.error;

	fileClose(&file);

	return result;
}

File dataFileOpen(const char *filename, const char *mode)
{
	if (dataDirPath == NULL)
		findDataFiles();

#ifndef NDEBUG
	for (size_t i = 0; filename[i] != '\0'; ++i)
		assert(!isupper(filename[i]));
#endif

	if (dataDirPathLen == 0)
		return fileOpen(filename, mode);

	size_t pathSize = dataDirPathLen + 1 + strlen(filename) + 1;
	char *path = malloc(pathSize);
	snprintf(path, pathSize, "%s/%s", dataDirPath, filename);

	File file = fileOpen(path, mode);

	free(path);

	return file;
}

static void makeDir(const char *path)
{
#ifdef _WIN32
	(void)_mkdir(path);
#else
	(void)mkdir(path, 0700);
#endif
}

// "dir/name" in a new string (just a copy of dir when name is NULL).
static char *joinPath(const char *dir, const char *name)
{
	size_t size = strlen(dir) + (name != NULL ? 1 + strlen(name) : 0) + 1;
	char *path = malloc(size);
	if (name != NULL)
		snprintf(path, size, "%s/%s", dir, name);
	else
		snprintf(path, size, "%s", dir);
	return path;
}

static bool copyFile(const char *fromPath, const char *toPath)
{
	FILE *from = fopen(fromPath, "rb");
	if (from == NULL)
		return false;
	FILE *to = fopen(toPath, "wb");
	if (to == NULL)
	{
		fclose(from);
		return false;
	}

	bool ok = true;
	char buffer[4096];
	size_t count;
	while ((count = fread(buffer, 1, sizeof(buffer), from)) > 0)
	{
		if (fwrite(buffer, 1, count, to) != count)
		{
			ok = false;
			break;
		}
	}
	if (ferror(from))
		ok = false;
	fclose(from);
	if (fclose(to) != 0)
		ok = false;

	if (!ok)
		remove(toPath);
	return ok;
}

// Copies dir/name from oldDir to newDir unless it is missing there or already
// in newDir.  False only when a copy failed.
static bool copyUserFile(const char *oldDir, const char *newDir, const char *name)
{
	char *oldPath = joinPath(oldDir, name);
	char *newPath = joinPath(newDir, name);
	bool ok = !fileExists(oldPath) || fileExists(newPath) || copyFile(oldPath, newPath);
	free(oldPath);
	free(newPath);
	return ok;
}

// Up to v1.0.4 Tyrian 2000+ kept its settings and saves in OpenTyrian2000's
// folder.  The first time its own folder is used, they are copied from
// there; the old folder is left as it was, so nothing can be lost and
// OpenTyrian2000 itself keeps its files.
static void migrateUserDir(const char *oldDir, const char *newDir)
{
	static const char *const mainFiles[] = { "tyrian.sav", "tyrian.cfg", "opentyrian.cfg" };
	static const char *const otherFiles[] = { "tyrian.sav.bak", "gamecontrollerdb.txt", "newsh$.shp" };
	const size_t mainCount = sizeof(mainFiles) / sizeof(*mainFiles);
	const size_t otherCount = sizeof(otherFiles) / sizeof(*otherFiles);

	// Only into a folder that has none of the main files yet.
	bool oldHasFiles = false;
	for (size_t i = 0; i < mainCount; ++i)
	{
		char *newPath = joinPath(newDir, mainFiles[i]);
		char *oldPath = joinPath(oldDir, mainFiles[i]);
		bool newExists = fileExists(newPath);
		oldHasFiles = oldHasFiles || fileExists(oldPath);
		free(newPath);
		free(oldPath);
		if (newExists)
			return;
	}
	if (!oldHasFiles)
		return;

	makeDir(newDir);

	for (size_t i = 0; i < otherCount; ++i)
		copyUserFile(oldDir, newDir, otherFiles[i]);

	char name[16];
	for (int i = 1; i <= 255; ++i)  // the recorded demos (see demo.c)
	{
		snprintf(name, sizeof(name), "demorec.%d", i);
		copyUserFile(oldDir, newDir, name);
	}

	// The main files go last.  If one fails, the ones already copied are
	// taken back, so that the copy is tried again next time.
	for (size_t i = 0; i < mainCount; ++i)
	{
		if (!copyUserFile(oldDir, newDir, mainFiles[i]))
		{
			for (size_t j = 0; j < i; ++j)
			{
				char *path = joinPath(newDir, mainFiles[j]);
				remove(path);
				free(path);
			}
			return;
		}
	}
}

static void determineUserDirPath(void)
{
	if (userDirPathLen != 0)
	{
		free(userDirPath);
		userDirPathLen = 0;
	}

	char *basePath = SDL_GetBasePath();
	if (basePath != NULL)
	{
		// If a certain file exists in the base path, store user files there.
		const char *const filename = "opentyrian.cfg";

		size_t filePathSize = strlen(basePath) + strlen(filename) + 1;
		char *filePath = malloc(filePathSize);
		snprintf(filePath, filePathSize, "%s%s", basePath, filename);

		bool portable = fileExists(filePath);
		
		free(filePath);

		if (portable)
		{
			userDirPathLen = strlen(basePath) - 1;  // Trim trailing slash.
			size_t userDirPathSize = userDirPathLen + 1;
			userDirPath = malloc(userDirPathSize);
			snprintf(userDirPath, userDirPathSize, "%s", basePath);
		}

		SDL_free(basePath);

		if (portable)
			return;
	}

	// Settings and saves go into a folder in the system's user settings
	// folder (APPDATA, or ~/.config on Linux).
	char *parent = NULL;
#ifdef TARGET_WIN32
	const char *appData = getenv("APPDATA");
	if (appData != NULL)
		parent = joinPath(appData, NULL);
	const char *const oldName = "OpenTyrian2000";
	const char *const newName = "Tyrian2000Plus";
#else
	const char *xdgConfigHome = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	if (xdgConfigHome != NULL)
		parent = joinPath(xdgConfigHome, NULL);
	else if (home != NULL)
		parent = joinPath(home, ".config");
	const char *const oldName = "opentyrian2000";
	const char *const newName = "tyrian2000plus";
#endif

	if (parent != NULL)
	{
		userDirPath = joinPath(parent, newName);
		userDirPathLen = strlen(userDirPath);

		char *oldDirPath = joinPath(parent, oldName);
		migrateUserDir(oldDirPath, userDirPath);
		free(oldDirPath);

		free(parent);
		return;
	}

	userDirPath = "";
	userDirPathLen = 0;
}

File userFileOpen(const char *filename, const char *mode)
{
	if (userDirPath == NULL)
		determineUserDirPath();

	if (userDirPathLen == 0)
		return fileOpen(filename, mode);

	makeDir(userDirPath);

	size_t pathSize = userDirPathLen + 1 + strlen(filename) + 1;
	char *path = malloc(pathSize);
	snprintf(path, pathSize, "%s/%s", userDirPath, filename);

	File file = fileOpen(path, mode);

	free(path);

	return file;
}

void fileSetPosition(File *file, long position)
{
	if (file->error)
		return;

	errno = 0;  // fseek might not set errno
	if (fseek(file->f, position, SEEK_SET) == 0)
		return;

	file->errnum = errno;
	file->error = true;
}

long fileGetPosition(File *file)
{
	if (file->error)
		return 0;

	errno = 0;  // ftell might not set errno
	long position = ftell(file->f);
	if (position >= 0)
		return position;

	file->errnum = errno;
	file->error = true;

	return 0;
}

long fileGetLength(File *file)
{
	if (file->error)
		return 0;

	errno = 0;  // fseek/ftell might not set errno
	long position = ftell(file->f);
	if (position >= 0 &&
	    fseek(file->f, 0, SEEK_END) == 0)
	{
		long length = ftell(file->f);
		if (length >= 0 &&
		    fseek(file->f, position, SEEK_SET) == 0)
		{
			return length;
		}
	}

	file->errnum = errno;
	file->error = true;

	return 0;
}

size_t fileReadAtMost(File *file, void *data, size_t size)
{
	if (file->error)
		return 0;

	errno = 0;  // fread might not set errno
	size_t read = fread(data, 1, size, file->f);
	if (read == size)
		return read;

	file->errnum = errno;
	file->error = ferror(file->f) != 0;
	assert(file->error || feof(file->f) != 0);

	return read;
}

void fileReadExactly(File *file, void *data, size_t size)
{
	if (file->error)
	{
		memset(data, 0, size);
		return;
	}

	errno = 0;  // fread might not set errno
	size_t read = fread(data, 1, size, file->f);
	if (read == size)
		return;

	file->errnum = errno;
	file->error = true;

	if (file->errnum == 0)
		file->errnum = ERRNUM_EOF;

	memset((uint8_t *)data + read, 0, size - read);
}

void fileWrite(File *file, const void *data, size_t size)
{
	if (file->error)
		return;

	errno = 0;  // fwrite might not set errno
	size_t written = fwrite(data, 1, size, file->f);
	if (written == size)
		return;

	file->errnum = errno;
	file->error = true;

	assert(written < size);
}

void fileFlush(File *file)
{
	if (file->error)
		return;

	errno = 0;  // fflush might not set errno
	if (fflush(file->f) == 0)
		return;

	file->errnum = errno;
	file->error = true;
}

void fileClose(File *file)
{
	if (file->f == NULL)
		return;

	errno = 0;  // fclose might not set errno
	int result = fclose(file->f);
	file->f = NULL;
	if (result == 0)
		return;

	file->errnum = errno;
	file->error = true;
}

const char *fileGetError(File *file)
{
	switch (file->errnum)
	{
	case ERRNUM_EOF:
		return "Unexpected end of file";
	case 0:
		if (file->error)
			return "Unknown error";
		// fall through
	default:
		return strerror(file->errnum);
	}
}
