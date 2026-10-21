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
#include "helptext.h"

#include "config.h"
#include "file.h"
#include "fonthand.h"
#include "joystick.h"
#include "lang.h"
#include "logging.h"
#include "menus.h"
#include "opentyr.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

const JE_byte menuHelp[MENU_MAX][11] = /* [1..maxmenu, 1..11] */
{
	{  1, 34,  2,  3,  4,  5,                  0, 0, 0, 0, 0 },
	{  6,  7,  8,  9, 10, 11, 11, 12,                0, 0, 0 },
	{ 13, 14, 15, 15, 16, 17, 35,  0, 12,               0, 0 },  // 0: Setup..., text in game_menu.c
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{                        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{  4, 30, 30,  3,  5,                   0, 0, 0, 0, 0, 0 },
	{  4, 37, 12,                     0, 0, 0, 0, 0, 0, 0, 0 },
	{ 16, 17, 15, 15, 12,                   0, 0, 0, 0, 0, 0 },
	{ 31, 31, 31, 31, 32, 12,                  0, 0, 0, 0, 0 },
	{  4, 34,  3,  5,                    0, 0, 0, 0, 0, 0, 0 },
	{ 35, 35, 35, 36, 12,                   0, 0, 0, 0, 0, 0 }
};

char helpTxt[39][231];                                                   /* [1..39] of string [230] */
char pName[21][16];                                                      /* [1..21] of string [15] */
char miscText[HELPTEXT_MISCTEXT_COUNT][42];                              /* [1..68] of string [41] */
char miscTextB[HELPTEXT_MISCTEXTB_COUNT][HELPTEXT_MISCTEXTB_SIZE];       /* [1..5] of string [10] */
char keyName[8][18];                                                     /* [1..8] of string [17] */
char menuText[10][HELPTEXT_MENUTEXT_SIZE];                               /* Continue, [1..7] of string [20], + Replay, About */
char outputs[9][31];                                                     /* [1..9] of string [30] */
char topicName[6][21];                                                   /* [1..6] of string [20] */
char mainMenuHelp[HELPTEXT_MAINMENUHELP_COUNT][66];                      /* [1..34] of string [65] */
char inGameText[6][21];                                                  /* [1..6] of string [20] */
char detailLevel[6][13];                                                 /* [1..6] of string [12] */
char gameSpeedText[5][13];                                               /* [1..5] of string [12] */
char inputDevices[3][13];                                                /* [1..3] of string [12] */
char networkText[HELPTEXT_NETWORKTEXT_COUNT][HELPTEXT_NETWORKTEXT_SIZE]; /* [1..4] of string [20] */
char difficultyNameB[11][21];                                            /* [0..9] of string [20] */
char joyButtonNames[5][21];                                              /* [1..5] of string [20] */
char superShips[HELPTEXT_SUPERSHIPS_COUNT][26];                          /* [0..10] of string [25] */
char specialName[HELPTEXT_SPECIALNAME_COUNT][10];                        /* [1..9] of string [9] */
char destructHelp[25][22];                                               /* [1..25] of string [21] */
char weaponNames[17][17];                                                /* [1..17] of string [16] */
char destructModeName[DESTRUCT_MODES][13];                               /* [1..destructmodes] of string [12] */
char shipInfo[HELPTEXT_SHIPINFO_COUNT][2][256];                          /* [1..13, 1..2] of string */
char licensingInfo[3][46];                                               /* [1..3] of string */
char orderingInfo[6][32];                                                /* [1..6] of string */
char superTyrianText[6][64];                                             /* [1..6] of string */
char menuInt[MENU_MAX+1][11][18];                                        /* [0..14, 1..11] of string [17] */

static void decrypt_string(char *s, size_t len)
{
	static const unsigned char crypt_key[] = { 204, 129, 63, 255, 71, 19, 25, 62, 1, 99 };

	if (len == 0)
		return;

	for (size_t i = len - 1; ; --i)
	{
		s[i] ^= crypt_key[i % sizeof(crypt_key)];
		if (i == 0)
			break;
		s[i] ^= s[i - 1];
	}
}

void readEncryptedString(File *file, char *dst, size_t size)
{
	Uint8 buffer[255];

	Uint8 len = fileReadU8(file);
	fileReadExactly(file, buffer, len);

	if (size == 0)
		return;

	decrypt_string((char *)buffer, len);

#ifndef NDEBUG
	// Some Tyrian 2000 strings are padded with trailing spaces (ex. "Canned Peaches"); only the
	// text before the padding has to fit.
	size_t textLen = len;
	while (textLen > 0 && buffer[textLen - 1] == ' ')
		--textLen;
	assert(textLen < size);
#endif
	len = MIN(len, size - 1);

	memcpy(dst, buffer, len);
	dst[len] = '\0';

	if (lang_translate_reads)
		lang_translate(dst, size);
}

// replaces every occurrence of find in text (at most size - 1 characters are kept)
static void replaceAll(char *text, size_t size, const char *find, const char *replacement)
{
	const size_t findLen = strlen(find);
	const size_t replacementLen = strlen(replacement);

	char *pos = text;
	while ((pos = strstr(pos, find)) != NULL)
	{
		const size_t tailLen = strlen(pos + findLen);
		if ((size_t)(pos - text) + replacementLen + tailLen >= size)
			break;

		memmove(pos + replacementLen, pos + findLen, tailLen + 1);
		memcpy(pos, replacement, replacementLen);
		pos += replacementLen;
	}
}

/*
 * The help texts in tyrian.hdt talk about the keyboard (Enter, ESC, arrow
 * keys...).  When a gamepad is connected, swap those for the controller's own
 * button names, read from the current controller settings.  Unknown text is
 * returned unchanged, so this is safe with any version of the data files.
 */
const char *padHelpText(const char *text)
{
	static char buffer[2][256];
	static unsigned int which = 0;

	const int j = ignore_joystick ? -1 : joystick_first_gamepad();
	if (j < 0 || text == NULL || text[0] == '\0')
		return text;

	char change[32], leftSidekick[32], rightSidekick[32];
	joystick_action_icons(j, 5, change, sizeof change);
	joystick_action_icons(j, 6, leftSidekick, sizeof leftSidekick);
	joystick_action_icons(j, 7, rightSidekick, sizeof rightSidekick);

	const char *const confirm = joystick_button_icon(j, SDL_CONTROLLER_BUTTON_A);
	const char *const cancel = joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B);

	char confirmButton[32], pressConfirm[32], PressConfirm[32], doneOrCancel[32], PressCancel[32];
	char changeButton[64], changeKey[40], sidekickButtons[80];
	snprintf(confirmButton, sizeof confirmButton, tr("the ~%s~ button"), confirm);
	snprintf(pressConfirm, sizeof pressConfirm, tr("press ~%s~"), confirm);
	snprintf(PressConfirm, sizeof PressConfirm, tr("Press %s"), confirm);
	snprintf(doneOrCancel, sizeof doneOrCancel, tr("done or ~%s~"), cancel);
	snprintf(PressCancel, sizeof PressCancel, tr("Press %s"), cancel);
	snprintf(changeButton, sizeof changeButton, tr("the ~%s~ button (~change fire~)"), change);
	snprintf(changeKey, sizeof changeKey, "[%s]", change);
	snprintf(sidekickButtons, sizeof sidekickButtons, tr("fired with ~%s~ and ~%s~"), leftSidekick, rightSidekick);

	const struct { const char *find, *replacement; } rules[] =
	{
		// keys that have no controller equivalent
		{ tr("  Use Alt-X for a quick way to quit the game."), "" },
		{ tr("  (Shortcut is ALT-L)"), "" },
		{ tr("  (Shortcut is ALT-S)"), "" },

		// Enter / space bar confirm, ESC cancels
		{ tr("the \"enter\" key or the space bar"), confirmButton },
		{ tr("press the Enter key or spacebar"), pressConfirm },
		{ tr("press Enter or hit the spacebar"), pressConfirm },
		{ tr("press Enter or the spacebar"), pressConfirm },
		{ tr("press Enter"), pressConfirm },
		{ tr("Press ENTER"), PressConfirm },
		{ tr("done or ESC"), doneOrCancel },
		{ tr("Press ESC"), PressCancel },
		{ tr("Press a key"), tr("Press a button") },

		// weapon buttons
		{ tr("the keyboard command ~change fire~ or a joystick button assigned to ~Change rear weapon~"), changeButton },
		{ tr("usually fired with separate buttons"), sidekickButtons },
		{ tr("[/]"), changeKey },

		// arrow keys
		{ tr("up/down arrow keys"), tr("D-pad up/down") },
		{ tr("right and left arrow keys"), tr("D-pad right/left") },
		{ tr("left and right arrow keys"), tr("D-pad left/right") },
		{ tr("left/right ~arrow keys~"), tr("~D-pad~ left/right") },
		{ tr("left/right arrow keys"), tr("D-pad left/right") },
		{ tr("Left/right arrow keys"), tr("D-pad left/right") },
		{ tr("Use arrow keys"), tr("Use the D-pad") },
		{ tr("arrow keys"), tr("D-pad") },
	};

	char *out = buffer[which];
	which = (which + 1) % COUNTOF(buffer);

	// at most 254 characters: JE_helpBox() keeps text positions in a byte
	const size_t size = sizeof buffer[0] - 1;

	SDL_strlcpy(out, text, size);

	for (size_t i = 0; i < COUNTOF(rules); ++i)
		replaceAll(out, size, rules[i].find, rules[i].replacement);

	return out;
}

void JE_helpBox(SDL_Surface *screen,  int x, int y, const char *message, JE_byte boxWidth, JE_byte verticalHeight, JE_byte color, JE_byte brightness, JE_byte shadeType)
{
	JE_byte startpos, endpos, pos;
	JE_boolean endstring;

	char substring[256];

	if (strlen(message) == 0)
	{
		return;
	}

	pos = 1;
	endpos = 0;
	endstring = false;

	do
	{
		startpos = endpos + 1;

		do
		{
			endpos = pos;
			do
			{
				pos++;
				if (pos == strlen(message))
				{
					endstring = true;
					if ((unsigned)(pos - startpos) < boxWidth)
					{
						endpos = pos + 1;
					}
				}

			} while (!(message[pos-1] == ' ' || endstring));

		} while (!((unsigned)(pos - startpos) > boxWidth || endstring));

		SDL_strlcpy(substring, message + startpos - 1, MIN((size_t)(endpos - startpos + 1), sizeof(substring)));
		JE_textShade(screen, x, y, substring, color, brightness, shadeType);

		y += verticalHeight;

	} while (!endstring);

	if (endpos != pos + 1)
	{
		JE_textShade(screen, x, y, message + endpos, color, brightness, shadeType);
	}
}

void JE_HBox(SDL_Surface *screen, int x, int y, JE_byte messageNum, JE_byte boxWidth, JE_byte verticalHeight, JE_byte color, JE_byte brightness)
{
	JE_helpBox(screen, x, y, padHelpText(helpTxt[messageNum-1]), boxWidth, verticalHeight, color, brightness, FULL_SHADE);
}

void JE_loadHelpText(void)
{
	static const unsigned int menuInt_entries[MENU_MAX + 1] =
	{
		-1, 7, 9, 9, -1, -1, 11, -1, -1, -1, 6, 4, 7, 7, 5, 6
	};
	static const unsigned int setup_entries[10] = { 10, 5, 4, 4, 5, 7, 7, 21, 3, 3 };
	
	const char *filename = "tyrian.hdt";

	File file = dataFileOpen(filename, "rb");
	if (file.error)
	{
		logFatal("Failed to open file '%s': %s", filename, fileGetError(&file));
		exit(EXIT_FAILURE);
	}

	// Tyrian 2000+: texts in the chosen language (see lang.h)
	lang_translate_reads = true;

	(void)fileReadU32(&file);  // Episode 1-3 item data position

	/*Online Help*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(helpTxt); ++i)
		readEncryptedString(&file, helpTxt[i], sizeof helpTxt[i]);
	readEncryptedString(&file, NULL, 0);

	/*Planet names*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(pName); ++i)
		readEncryptedString(&file, pName[i], sizeof pName[i]);
	readEncryptedString(&file, NULL, 0);

	/*Miscellaneous text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(miscText); ++i)
		readEncryptedString(&file, miscText[i], sizeof miscText[i]);
	readEncryptedString(&file, NULL, 0);

	/*Little Miscellaneous text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(miscTextB); ++i)
		readEncryptedString(&file, miscTextB[i], sizeof miscTextB[i]);
	readEncryptedString(&file, NULL, 0);

	/*Key names*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[6]; ++i)
		readEncryptedString(&file, menuInt[6][i], sizeof menuInt[6][i]);
	readEncryptedString(&file, NULL, 0);

	/*Main Menu*/
	readEncryptedString(&file, NULL, 0);
	// Tyrian2000 Plus: Continue first (titleScreen() hides it when there is
	// no saved game)
	SDL_strlcpy(menuText[0], tr("Continue"), sizeof(menuText[0]));
	for (size_t i = 1; i < 8; ++i)
		readEncryptedString(&file, menuText[i], sizeof menuText[i]);
	readEncryptedString(&file, NULL, 0);

	// OpenTyrian2000 Override
	strcpy(menuText[7], menuText[6]);
	strcpy(menuText[6], menuText[5]);
	SDL_strlcpy(menuText[5], tr("Setup"), sizeof(menuText[5]));

	// Tyrian2000 Plus: Replay after Demo, About before Quit
	strcpy(menuText[9], menuText[7]);
	SDL_strlcpy(menuText[7], tr("Replay"), sizeof(menuText[7]));
	SDL_strlcpy(menuText[8], tr("About"), sizeof(menuText[8]));

	/*Event text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(outputs); ++i)
		readEncryptedString(&file, outputs[i], sizeof outputs[i]);
	readEncryptedString(&file, NULL, 0);

	/*Help topics*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(topicName); ++i)
		readEncryptedString(&file, topicName[i], sizeof topicName[i]);
	readEncryptedString(&file, NULL, 0);

	/*Main Menu Help*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(mainMenuHelp); ++i)
		readEncryptedString(&file, mainMenuHelp[i], sizeof mainMenuHelp[i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 1 - Main*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[1]; ++i)
		readEncryptedString(&file, menuInt[1][i], sizeof menuInt[1][i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 2 - Items*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[2]; ++i)
		readEncryptedString(&file, menuInt[2][i], sizeof menuInt[2][i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 3 - Options*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[3]; ++i)
		readEncryptedString(&file, menuInt[3][i], sizeof menuInt[3][i]);
	readEncryptedString(&file, NULL, 0);
	// Tyrian2000 Plus: "Setup..." (the title screen setup menu) goes before "Done"
	strcpy(menuInt[3][9], menuInt[3][8]);
	SDL_strlcpy(menuInt[3][8], tr("Setup..."), sizeof(menuInt[3][8]));

	/*InGame Menu*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(inGameText); ++i)
		readEncryptedString(&file, inGameText[i], sizeof inGameText[i]);
	readEncryptedString(&file, NULL, 0);

	/*Detail Level*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(detailLevel); ++i)
		readEncryptedString(&file, detailLevel[i], sizeof detailLevel[i]);
	readEncryptedString(&file, NULL, 0);

	/*Game speed text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(gameSpeedText); ++i)
		readEncryptedString(&file, gameSpeedText[i], sizeof gameSpeedText[i]);
	readEncryptedString(&file, NULL, 0);

	// episode names
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(episode_name); ++i)
		readEncryptedString(&file, episode_name[i], sizeof episode_name[i]);
	readEncryptedString(&file, NULL, 0);

	// difficulty names
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(difficulty_name); ++i)
		readEncryptedString(&file, difficulty_name[i], sizeof difficulty_name[i]);
	readEncryptedString(&file, NULL, 0);

	// gameplay mode names
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(gameplay_name); ++i)
		readEncryptedString(&file, gameplay_name[i], sizeof gameplay_name[i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 10 - 2Player Main*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[10]; ++i)
		readEncryptedString(&file, menuInt[10][i], sizeof menuInt[10][i]);
	readEncryptedString(&file, NULL, 0);

	/*Input Devices*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(inputDevices); ++i)
		readEncryptedString(&file, inputDevices[i], sizeof inputDevices[i]);
	readEncryptedString(&file, NULL, 0);

	/*Network text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(networkText); ++i)
		readEncryptedString(&file, networkText[i], sizeof networkText[i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 11 - 2Player Network*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[11]; ++i)
		readEncryptedString(&file, menuInt[11][i], sizeof menuInt[11][i]);
	readEncryptedString(&file, NULL, 0);

	/*HighScore Difficulty Names*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(difficultyNameB); ++i)
		readEncryptedString(&file, difficultyNameB[i], sizeof difficultyNameB[i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 12 - Network Options*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[12]; ++i)
		readEncryptedString(&file, menuInt[12][i], sizeof menuInt[12][i]);
	readEncryptedString(&file, NULL, 0);

	/*Menu 13 - Joystick*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[13]; ++i)
		readEncryptedString(&file, menuInt[13][i], sizeof menuInt[13][i]);
	readEncryptedString(&file, NULL, 0);

	/*Joystick Button Assignments*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(joyButtonNames); ++i)
		readEncryptedString(&file, joyButtonNames[i], sizeof joyButtonNames[i]);
	readEncryptedString(&file, NULL, 0);

	/*SuperShips - For Super Arcade Mode*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(superShips); ++i)
		readEncryptedString(&file, superShips[i], sizeof superShips[i]);
	readEncryptedString(&file, NULL, 0);

	/*SuperShips - For Super Arcade Mode*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(specialName); ++i)
		readEncryptedString(&file, specialName[i], sizeof specialName[i]);
	readEncryptedString(&file, NULL, 0);

	/*Secret DESTRUCT game*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(destructHelp); ++i)
		readEncryptedString(&file, destructHelp[i], sizeof destructHelp[i]);
	readEncryptedString(&file, NULL, 0);

	/*Secret DESTRUCT weapons*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(weaponNames); ++i)
		readEncryptedString(&file, weaponNames[i], sizeof weaponNames[i]);
	readEncryptedString(&file, NULL, 0);

	/*Secret DESTRUCT modes*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(destructModeName); ++i)
		readEncryptedString(&file, destructModeName[i], sizeof destructModeName[i]);
	readEncryptedString(&file, NULL, 0);

	/*NEW: Ship Info*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(shipInfo); ++i)
	{
		readEncryptedString(&file, shipInfo[i][0], sizeof shipInfo[i][0]);
		readEncryptedString(&file, shipInfo[i][1], sizeof shipInfo[i][1]);
	}
	readEncryptedString(&file, NULL, 0);

	/*Menu 14 - Super Tyrian*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[14]; ++i)
		readEncryptedString(&file, menuInt[14][i], sizeof menuInt[14][i]);
	readEncryptedString(&file, NULL, 0);

	/*Timed Battle Planets*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(timed_battle_name); ++i)
		readEncryptedString(&file, timed_battle_name[i], sizeof timed_battle_name[i]);
	readEncryptedString(&file, NULL, 0);

	/*Setup entries, skipped since we don't use them*/
	for (size_t entry = 0; entry < COUNTOF(setup_entries); ++entry)
	{
		readEncryptedString(&file, NULL, 0);
		for (size_t i = 0; i < setup_entries[entry]; ++i)
			readEncryptedString(&file, NULL, 0);
		readEncryptedString(&file, NULL, 0);
	}

	/*Menu 15 - Mouse Settings*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < menuInt_entries[15]; ++i)
		readEncryptedString(&file, menuInt[15][i], sizeof menuInt[15][i]);
	readEncryptedString(&file, NULL, 0);

	/*Tyrian Licensing Info*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(licensingInfo); ++i)
		readEncryptedString(&file, licensingInfo[i], sizeof licensingInfo[i]);
	readEncryptedString(&file, NULL, 0);

	/*Default High Score Names*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(defaultHighScoreNames); ++i)
		readEncryptedString(&file, defaultHighScoreNames[i], sizeof defaultHighScoreNames[i]);
	readEncryptedString(&file, NULL, 0);

	/*Default Team Names*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(defaultTeamNames); ++i)
		readEncryptedString(&file, defaultTeamNames[i], sizeof defaultTeamNames[i]);
	readEncryptedString(&file, NULL, 0);

	/*Ordering Info?*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(orderingInfo); ++i)
		readEncryptedString(&file, orderingInfo[i], sizeof orderingInfo[i]);
	readEncryptedString(&file, NULL, 0);

	/*Super Tyrian text*/
	readEncryptedString(&file, NULL, 0);
	for (size_t i = 0; i < COUNTOF(superTyrianText); ++i)
		readEncryptedString(&file, superTyrianText[i], sizeof superTyrianText[i]);

	lang_translate_reads = false;

	if (file.error)
	{
		logFatal("Failed to read from file '%s': %s", filename, fileGetError(&file));
		exit(EXIT_FAILURE);
	}

	fileClose(&file);
}
