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

/* Headless test for a tyrian.sav put in the user folder while the game runs
 * (saved games brought from another computer or the browser): the title
 * screen reads it again, and closing the game does not write over it.
 * Run with XDG_CONFIG_HOME set to an empty folder. */
#include "config.h"
#include "file.h"

#include <stdio.h>
#include <string.h>

static int fails = 0;

static void check(bool ok, const char *what)
{
	printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
	fails += !ok;
}

static size_t readSav(Uint8 *data, size_t size)
{
	File file = userFileOpen("tyrian.sav", "rb");
	size_t n = file.error ? 0 : fileReadAtMost(&file, data, size);
	fileClose(&file);
	return n;
}

static void writeSav(const Uint8 *data, size_t size)
{
	File file = userFileOpen("tyrian.sav", "wb");
	fileWrite(&file, data, size);
	fileClose(&file);
}

static void nameSlot(const char *name)
{
	snprintf(saveFiles[0].name, sizeof(saveFiles[0].name), "%-14s", name);
	saveFiles[0].level = 1;
}

int main(void)
{
	static Uint8 a[8192], b[8192], now[8192];

	loadSaves();  // none yet: the defaults
	nameSlot("FROM BROWSER");
	saveSaves();
	const size_t sizeA = readSav(a, sizeof(a));
	check(sizeA == 4722, "tyrian.sav written, 4722 bytes");

	nameSlot("ON THIS PC");
	saveSaves();
	const size_t sizeB = readSav(b, sizeof(b));
	check(sizeB == sizeA && memcmp(a, b, sizeA) != 0, "a second, different tyrian.sav");

	reloadSavesIfReplaced();
	check(strncmp(saveFiles[0].name, "ON THIS PC", 10) == 0, "unchanged file: nothing read again");

	writeSav(a, sizeA);  // copied in from elsewhere
	reloadSavesIfReplaced();
	check(strncmp(saveFiles[0].name, "FROM BROWSER", 12) == 0, "replaced file: read again");

	writeSav(b, sizeB);  // copied in again, and then the game closes
	saveSavesUnlessReplaced();
	check(readSav(now, sizeof(now)) == sizeB && memcmp(now, b, sizeB) == 0, "closing keeps the file copied in");

	loadSaves();
	nameSlot("PLAYED MORE");
	saveSaves();
	nameSlot("PLAYED EVEN MORE");
	saveSavesUnlessReplaced();
	loadSaves();
	check(strncmp(saveFiles[0].name, "PLAYED EVEN MORE", 14) == 0, "closing still saves when nothing was copied in");

	printf(fails ? "%d FAILED\n" : "all passed\n", fails);
	return fails != 0;
}
