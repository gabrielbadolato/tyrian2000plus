/* Usage: XDG_CONFIG_HOME=<dir> test_saves <dir>/tyrian2000plus
 * Loads a real Tyrian 2000 TYRIAN.SAV/TYRIAN.CFG with the merged code, saves them again,
 * and checks that the files come out byte-for-byte identical. */
#include "config.h"
#include "helptext.h"
#include "file.h"
#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static long slurp(const char *path, unsigned char **out)
{
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
	*out = malloc(n); fread(*out, 1, n, f); fclose(f);
	return n;
}

static int compare(const char *name, const char *a, const char *b, long ignore)
{
	unsigned char *x, *y;
	long nx = slurp(a, &x), ny = slurp(b, &y);
	if (nx != ny) { printf("  FAIL %s: size %ld vs %ld\n", name, nx, ny); return 1; }
	long diffs = 0, first = -1;
	for (long i = 0; i < nx; ++i) if (i != ignore && x[i] != y[i]) { if (first < 0) first = i; diffs++; }
	if (diffs) { printf("  FAIL %s: %ld differing bytes (first at %ld)\n", name, diffs, first); return 1; }
	printf("  ok   %s identical (%ld bytes)\n", name, nx);
	return 0;
}

int main(int argc, char **argv)
{
	const char *dir = argv[1];  // user dir holding the original files
	char a[512], b[512];
	int fails = 0;

	JE_loadHelpText();
	loadConfiguration();
	loadSaves();

	printf("  slot 1 name: '%s'  high score #1 (ep1): %d by '%s'\n",
	       saveFiles[0].name, t2kHighScores[10][0].score, t2kHighScores[10][0].playerName);
	printf("  timed battle #1: %d by '%s'\n", t2kHighScores[0][0].score, t2kHighScores[0][0].playerName);

	snprintf(a, sizeof a, "%s/tyrian.sav", dir); snprintf(b, sizeof b, "%s/orig.sav", dir); rename(a, b);
	snprintf(a, sizeof a, "%s/tyrian.cfg", dir); snprintf(b, sizeof b, "%s/orig.cfg", dir); rename(a, b);

	saveSaves();
	saveConfiguration();

	snprintf(a, sizeof a, "%s/tyrian.sav", dir); snprintf(b, sizeof b, "%s/orig.sav", dir); fails += compare("tyrian.sav", a, b, -1);
	snprintf(a, sizeof a, "%s/tyrian.cfg", dir); snprintf(b, sizeof b, "%s/orig.cfg", dir); fails += compare("tyrian.cfg", a, b, 3);  // byte 3 (jConfigure): the game normalizes 0 to 1, like DOS Tyrian
	(void)argc;
	printf(fails ? "FAILED\n" : "ALL PASSED\n");
	return fails;
}
