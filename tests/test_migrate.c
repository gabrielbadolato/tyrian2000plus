/* Headless test for the copy of the saves from OpenTyrian2000's folder.
 *
 * Usage: XDG_CONFIG_HOME=<dir> test_migrate <dir> {fail|ok}, once per mode,
 * each in a new process (the user folder is chosen once).
 * <dir>/opentyrian2000 holds a tyrian.sav.
 *   fail: <dir>/opentyrian2000/tyrian.cfg is a folder, so its copy fails; the
 *         game must use the old folder and leave the new one without saves.
 *   ok:   the copy works and the save is read from the new folder. */
#include "file.h"

#include <stdio.h>
#include <string.h>

static int fails = 0;

static void check(bool ok, const char *what)
{
	printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
	fails += !ok;
}

static bool exists(const char *dir, const char *name)
{
	char path[512];
	snprintf(path, sizeof path, "%s/%s", dir, name);
	FILE *f = fopen(path, "rb");
	if (f != NULL)
		fclose(f);
	return f != NULL;
}

int main(int argc, char *argv[])
{
	if (argc != 3)
		return 2;
	const char *dir = argv[1];
	const bool failMode = strcmp(argv[2], "fail") == 0;

	char newDir[512];
	snprintf(newDir, sizeof newDir, "%s/tyrian2000plus", dir);

	char text[16] = "";
	File file = userFileOpen("tyrian.sav", "rb");
	check(!file.error, "tyrian.sav opens");
	if (!file.error)
	{
		fileReadAtMost(&file, text, sizeof text - 1);
		fileClose(&file);
	}
	check(strcmp(text, "old save") == 0, "it holds the old save");

	if (failMode)
		check(!exists(newDir, "tyrian.sav"), "a failed copy leaves no saves in the new folder");
	else
		check(exists(newDir, "tyrian.sav") && exists(newDir, "tyrian.cfg"), "the saves were copied to the new folder");

	printf(fails ? "FAILED\n" : "ALL PASSED\n");
	return fails != 0;
}
