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
/* Test of the Gallery (src/gallery.c).
 *
 * Without arguments, headless and without the game's data: makes pretend
 * enemies from a made-up sprite sheet as a level would, and checks what
 * gallery.cfg records (one entry per look, the pieces of a boss as one, the
 * invisible pieces left out, the enemies destroyed), and that it reads back.
 *
 *   XDG_CONFIG_HOME=<empty dir> test_gallery
 *
 * With the game's data, shows the Gallery screen with the gallery.cfg of the
 * user folder and saves pictures of it.  Each argument after the data folder
 * is done when the screen is shown: a key (an SDL scancode name, such as
 * Right or Tab), "click=<x>,<y>" (on the game's 320x200 screen), "wait" (a
 * frame), or "shot=<file.bmp>".  Built with
 * --wrap=JE_showVGA (see run_tests.sh).  No episode is loaded, as on the
 * title screen; "ep=<n>" right after the data folder loads one, as in the
 * ship's menu.  With "setup" (after it) it starts at the Setup menu instead
 * (Gallery... is there).
 *
 *   XDG_CONFIG_HOME=<dir> test_gallery <data-dir> wait wait shot=a.bmp Tab shot=b.bmp
 */
#include "config.h"
#include "episodes.h"
#include "file.h"
#include "gallery.h"
#include "gallery_catalog.h"
#include "helptext.h"
#include "keyboard.h"
#include "lang.h"
#include "opentyr.h"
#include "palette.h"
#include "player.h"
#include "sprite.h"
#include "stats.h"
#include "tyrian2.h"
#include "varz.h"
#include "video.h"

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

/* -------- the screen, with the game's data -------- */

static char **script;
static int scriptLeft;

void __real_JE_showVGA(void);
void __wrap_JE_showVGA(void);

void __wrap_JE_showVGA(void)
{
	__real_JE_showVGA();

	while (scriptLeft > 0)
	{
		const char *step = *script++;
		--scriptLeft;

		if (strcmp(step, "wait") == 0)
			return;

		if (strncmp(step, "shot=", 5) == 0)
		{
			SDL_Surface *shot = SDL_CreateRGBSurfaceWithFormat(0, VGAScreen->w, VGAScreen->h, 8, SDL_PIXELFORMAT_INDEX8);
			SDL_SetPaletteColors(shot->format->palette, colors, 0, 256);
			for (int y = 0; y < VGAScreen->h; ++y)
				memcpy((Uint8 *)shot->pixels + y * shot->pitch, (Uint8 *)VGAScreen->pixels + y * VGAScreen->pitch, VGAScreen->w);
			SDL_SaveBMP(shot, step + 5);
			SDL_FreeSurface(shot);
			printf("  saved %s\n", step + 5);
			continue;
		}

		int cx, cy;
		if (sscanf(step, "click=%d,%d", &cx, &cy) == 2)
		{
			// a left click at (cx, cy) of the game's screen
			mouseX = cx;
			mouseY = cy;
			Sint32 wx = cx, wy = cy;
			mapScreenPointToWindow(&wx, &wy);
			SDL_Event e = { 0 };
			e.type = SDL_MOUSEBUTTONDOWN;
			e.button.button = SDL_BUTTON_LEFT;
			e.button.state = SDL_PRESSED;
			e.button.x = wx + 1;  // the middle of the pixel
			e.button.y = wy + 1;
			SDL_PushEvent(&e);
			e.type = SDL_MOUSEBUTTONUP;
			e.button.state = SDL_RELEASED;
			SDL_PushEvent(&e);
			return;
		}

		SDL_Scancode scancode = SDL_GetScancodeFromName(step);
		SDL_Keymod mod = KMOD_NONE;
		if (strncmp(step, "Shift+", 6) == 0)
		{
			scancode = SDL_GetScancodeFromName(step + 6);
			mod = KMOD_LSHIFT;
		}
		if (scancode == SDL_SCANCODE_UNKNOWN)
		{
			printf("  unknown step '%s'\n", step);
			continue;
		}

		SDL_Event e = { 0 };
		e.type = SDL_KEYDOWN;
		e.key.keysym.scancode = scancode;
		e.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
		e.key.keysym.mod = mod;
		SDL_PushEvent(&e);
		e.type = SDL_KEYUP;
		SDL_PushEvent(&e);
		return;
	}

	if (scriptLeft == 0)
	{
		// done: leave the screen
		SDL_Event e = { 0 };
		e.type = SDL_KEYDOWN;
		e.key.keysym.scancode = SDL_SCANCODE_ESCAPE;
		e.key.keysym.sym = SDLK_ESCAPE;
		SDL_PushEvent(&e);
		scriptLeft = -1;
	}
}

static int showScreen(int argc, char **argv)
{
	customDataDirPath = argv[1];
	script = argv + 2;
	scriptLeft = argc - 2;
	// "ep=N": an episode loaded, as from the ship's menu (else as from the title screen)
	int episode = 0;
	if (scriptLeft > 0 && sscanf(*script, "ep=%d", &episode) == 1)
	{
		++script;
		--scriptLeft;
	}
	const bool setup = scriptLeft > 0 && strcmp(*script, "setup") == 0;
	if (setup)
	{
		++script;
		--scriptLeft;
	}

	if (SDL_Init(0) != 0)
		return EXIT_FAILURE;
	atexit(SDL_Quit);

	loadConfiguration();
	lang_set(lang_setting);

	if (!findDataFiles())
	{
		fprintf(stderr, "no game data in %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	JE_loadHelpText();
	JE_scanForEpisodes();
	if (episode > 0)
		JE_initEpisode((JE_byte)episode);

	init_video();
	init_keyboard();
	loadPals();
	JE_loadMainShapeTables("tyrian.shp");
	JE_loadCompShapes(&shopSpriteSheet, '1');

	gallery_load();
	stats_load();
	if (setup)
		setupMenu();
	else
		galleryScreen();
	return fails;
}

/* -------- recording, without data -------- */

// A sheet of 40 sprites, each one row of 12 pixels, but number 2, which is empty.
static Uint8 sheetData[40 * 2 + 40 * 16];

static void makeSheet(void)
{
	size_t at = 40 * 2;
	for (unsigned int i = 0; i < 40; ++i)
	{
		((Uint16 *)sheetData)[i] = SDL_SwapLE16((Uint16)at);
		if (i != 1)
		{
			sheetData[at++] = 0xc0;  // 12 pixels
			for (int p = 0; p < 12; ++p)
				sheetData[at++] = (Uint8)(i + 1);
			sheetData[at++] = 0x00;  // next row
		}
		sheetData[at++] = 0x0f;  // end
	}
	enemySpriteSheets[0].data = sheetData;
	enemySpriteSheets[0].size = at;
	enemySpriteSheetIds[0] = 5;
}

static void makeEnemy(unsigned int slot, unsigned int gr, unsigned int size, int x, int y, unsigned int linknum, unsigned int armor)
{
	memset(&enemy[slot], 0, sizeof(enemy[slot]));
	enemy[slot].sprite2s = &enemySpriteSheets[0];
	enemy[slot].egr[0] = (JE_word)gr;
	enemy[slot].ani = 1;
	enemy[slot].enemycycle = 1;
	enemy[slot].size = (JE_byte)size;
	enemy[slot].ex = (JE_integer)x;
	enemy[slot].ey = (JE_integer)y;
	enemy[slot].linknum = (JE_byte)linknum;
	enemy[slot].enemytype = (JE_word)(100 + slot);
	enemyDat[100 + slot].armor = (JE_byte)armor;
	enemyAvail[slot] = 0;
	gallery_enemy_made(slot);
}

static void kill(unsigned int slot)
{
	enemyAvail[slot] = 1;
	gallery_enemy_killed(slot);
}

static int recordTest(void)
{
	gallery_test_no_list = true;  // the fake enemies are not the game's
	makeSheet();
	episodeNum = 1;
	snprintf(levelName, sizeof(levelName), "%-10s", "TYRIAN");
	for (int i = 0; i < 100; ++i)
		enemyAvail[i] = 1;

	// items with a picture (the data also has some without, never shown)
	weaponPort[1].itemgraphic = weaponPort[4].itemgraphic = 7;
	shields[3].itemgraphic = 129;
	special[3].itemgraphic = 275;
	weaponPort[16].itemgraphic = 0;

	gallery_load();
	check(gallery_seen_count(GALLERY_ENEMIES) == 0, "nothing seen at first");

	memset(&player[0].items, 0, sizeof(player[0].items));
	player[0].items.ship = 1;
	player[0].items.weapon[0].id = 1;
	player[0].items.special = 3;

	gallery_level_start();
	check(gallery_seen_count(GALLERY_SHIPS) == 1, "the player's ship is seen");
	check(gallery_seen_count(GALLERY_OTHER) == 1, "the player's special weapon is seen");

	// the upgrade menu: ships 1 and 2, front weapons 1 and 4, a shield
	memset(itemAvail, 0, sizeof(itemAvail));
	memset(itemAvailMax, 0, sizeof(itemAvailMax));
	itemAvail[0][0] = 1; itemAvail[0][1] = 2; itemAvailMax[0] = 2;
	itemAvail[1][0] = 1; itemAvail[1][1] = 4; itemAvail[1][2] = 16; itemAvailMax[1] = 3;
	itemAvail[8][0] = 3; itemAvailMax[8] = 1;
	gallery_shop_items();
	check(gallery_seen_count(GALLERY_SHIPS) == 2, "the menu's ships are seen");
	check(gallery_seen_count(GALLERY_WEAPONS) == 2, "the menu's weapons are seen, not one without a picture");
	check(gallery_seen_count(GALLERY_OTHER) == 2, "the menu's shield is seen");

	// two enemies that look the same; recorded when they are gone
	makeEnemy(0, 1, 0, 10, 10, 0, 10);
	makeEnemy(1, 1, 0, 200, 50, 0, 10);
	gallery_level_frame();
	check(gallery_seen_count(GALLERY_ENEMIES) == 0, "not recorded while it is there");
	enemyAvail[1] = 1;  // left the screen
	gallery_level_frame();
	check(gallery_seen_count(GALLERY_ENEMIES) == 1, "two enemies that look the same are one");

	// a boss of four 2x2 pieces and an invisible one, all linked
	makeEnemy(2, 3, 1, 100, 0, 7, 50);
	makeEnemy(3, 5, 1, 124, 0, 7, 50);
	makeEnemy(4, 7, 1, 100, 28, 7, 50);
	makeEnemy(5, 9, 1, 124, 28, 7, 255);
	makeEnemy(6, 2, 0, 112, 10, 7, 254);
	// linked too, but far from the boss: a formation of another enemy
	makeEnemy(7, 11, 0, 10, 100, 7, 5);
	makeEnemy(8, 11, 0, 60, 100, 7, 5);
	gallery_level_frame();
	for (int i = 2; i <= 8; ++i)
		enemyAvail[i] = 1;
	gallery_level_frame();
	check(gallery_seen_count(GALLERY_ENEMIES) == 3, "the boss is one, the formation another");

	// the same boss again, elsewhere
	makeEnemy(10, 3, 1, 40, 60, 3, 50);
	makeEnemy(11, 5, 1, 64, 60, 3, 50);
	makeEnemy(12, 7, 1, 40, 88, 3, 50);
	makeEnemy(13, 9, 1, 64, 88, 3, 255);
	gallery_level_frame();

	// a lone invisible enemy is not an enemy to show
	makeEnemy(14, 2, 0, 0, -100, 0, 254);
	gallery_level_frame();
	enemyAvail[14] = 1;
	gallery_level_frame();
	check(gallery_seen_count(GALLERY_ENEMIES) == 3, "an invisible enemy is left out");

	// a building on the ground that scrolls in row by row: one thing
	makeEnemy(25, 15, 0, 150, 0, 0, 20);
	makeEnemy(26, 17, 0, 162, 0, 0, 20);
	gallery_level_frame();
	enemy[25].ey += 14;
	enemy[26].ey += 14;
	makeEnemy(27, 19, 0, 150, 0, 0, 20);
	makeEnemy(28, 21, 0, 162, 0, 0, 20);
	gallery_level_frame();
	kill(26);
	enemy[25].ey += 14;
	enemy[27].ey += 14;
	enemy[28].ey += 14;
	gallery_level_frame();
	check(gallery_seen_count(GALLERY_ENEMIES) == 3, "the building is not recorded while a piece is there");
	enemyAvail[25] = 1;
	enemyAvail[27] = 1;
	enemyAvail[28] = 1;
	gallery_level_frame();
	check(gallery_seen_count(GALLERY_ENEMIES) == 4, "the rows of a building are one enemy");

	// launched by the boss, on top of it: an enemy of its own
	makeEnemy(29, 23, 0, 44, 64, 0, 2);
	gallery_enemy_launched(29);
	gallery_level_frame();
	enemyAvail[29] = 1;
	gallery_level_frame();
	check(gallery_seen_count(GALLERY_ENEMIES) == 5, "an enemy launched by another is not a piece of it");

	kill(0);
	kill(10);
	kill(11);
	kill(12);
	makeEnemy(15, 13, 0, 150, 150, 0, 1);
	kill(15);  // made and destroyed in the same frame
	kill(13);
	gallery_level_end();

	gallery_load();
	check(gallery_seen_count(GALLERY_ENEMIES) == 6, "gallery.cfg reads back the enemies");
	check(gallery_seen_count(GALLERY_SHIPS) == 2 && gallery_seen_count(GALLERY_WEAPONS) == 2 &&
	      gallery_seen_count(GALLERY_OTHER) == 2, "gallery.cfg reads back the items");

	// what gallery.cfg says
	char path[512];
	const char *home = getenv("XDG_CONFIG_HOME");
	snprintf(path, sizeof(path), "%s/tyrian2000plus/gallery.cfg", home != NULL ? home : ".");
	FILE *f = fopen(path, "r");
	check(f != NULL, "gallery.cfg was written");
	if (f != NULL)
	{
		char text[8192];
		const size_t n = fread(text, 1, sizeof(text) - 1, f);
		text[n] = '\0';
		fclose(f);
		check(strstr(text, "5:3:1:0:0 5:5:1:24:0 5:7:1:0:28 5:9:1:24:28") != NULL, "the boss's pieces, from its top left");
		check(strstr(text, "5:19:0:0:0 5:21:0:12:0 5:15:0:0:14 5:17:0:12:14") != NULL, "the building's rows, from its top left");
		check(strstr(text, "'killed' '1'") != NULL, "an enemy and the boss counted destroyed once");
		check(strstr(text, "'indestructible' 'yes'") != NULL, "the piece that cannot be destroyed");
		check(strstr(text, "'level' 'TYRIAN'\n") != NULL, "the level where it was met");
	}

	// the game's list (src/gallery_catalog.h): a thing met is the entries it
	// is made of, by the enemyDat index of its pieces
	gallery_test_no_list = false;
	remove(path);
	gallery_load();
	check(gallery_seen_count(GALLERY_ENEMIES) == 0, "the list, none seen");
	gallery_level_start();
	int one = -1, other = -1;  // two enemies of episodes 1-3, one piece each
	for (size_t k = 0; k < COUNTOF(catalogTypes) && other < 0; ++k)
	{
		const CatalogType *t = &catalogTypes[k];
		if (t->dataset != 1 || catalogEnemies[t->enemy].parts != 1)
			continue;
		if (one < 0)
			one = (int)k;
		else if (catalogTypes[one].enemy != t->enemy)
			other = (int)k;
	}
	check(other >= 0, "the list has enemies");
	if (other >= 0)
	{
		// side by side on the ground, made together: one thing, two enemies
		makeEnemy(30, 1, 1, 10, 10, 0, 10);
		makeEnemy(31, 3, 1, 34, 10, 0, 10);
		enemy[30].enemytype = catalogTypes[one].type;
		enemy[31].enemytype = catalogTypes[other].type;
		enemyDat[catalogTypes[one].type].armor = 10;
		enemyDat[catalogTypes[other].type].armor = 10;
		gallery_level_frame();
		kill(30);
		kill(31);
		gallery_level_frame();
		check(gallery_seen_count(GALLERY_ENEMIES) == 2, "two enemies of the list side by side are two");
		gallery_level_end();
		gallery_load();
		check(gallery_seen_count(GALLERY_ENEMIES) == 2, "gallery.cfg reads back the list's enemies");
	}

	printf(fails ? "FAILED\n" : "all passed\n");
	return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}

int main(int argc, char **argv)
{
	if (argc >= 2)
		return showScreen(argc, argv);
	return recordTest();
}
