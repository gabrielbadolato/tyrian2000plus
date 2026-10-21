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
#include "config.h"

#include "episodes.h"
#include "file.h"
#include "joystick.h"
#include "lang.h"
#include "logging.h"
#include "loudness.h"
#include "memreader.h"
#include "memwriter.h"
#include "mtrand.h"
#include "oplchip.h"
#include "nortsong.h"
#include "opentyr.h"
#include "player.h"
#include "smooth.h"
#include "update_check.h"
#include "whatsnew.h"
#include "varz.h"
#include "video.h"
#include "video_scale.h"

#define SAVE_FILES_SIZE (109 * SAVE_FILES_NUM)
#define SAVE_FILE_SIZE (SAVE_FILES_SIZE + 100)

/* Configuration Load/Save handler */

static const Uint8 cryptKey[10] /* [1..10] */ =
{
	15, 50, 89, 240, 147, 34, 86, 9, 32, 208
};

const KeySettings defaultKeySettings =
{
	SDL_SCANCODE_UP,
	SDL_SCANCODE_DOWN,
	SDL_SCANCODE_LEFT,
	SDL_SCANCODE_RIGHT,
	SDL_SCANCODE_SPACE,
	SDL_SCANCODE_RETURN,
	SDL_SCANCODE_LCTRL,
	SDL_SCANCODE_LALT,
};

static const char *const keySettingNames[] =
{
	"up",
	"down",
	"left",
	"right",
	"fire",
	"change fire",
	"left sidekick",
	"right sidekick",
};

const MouseSettings defaultMouseSettings =
{
	1, 4, 5
};

static const char *const mouseSettingNames[] =
{
	"left mouse",
	"right mouse",
	"middle mouse",
};

static const char *const mouseSettingValues[] =
{
	"fire main weapon",
	"fire left sidekick",
	"fire right sidekick",
	"fire both sidekicks",
	"change rear mode",
};

// Tyrian 2000 loads these from tyrian.hdt (see JE_loadHelpText).
char defaultHighScoreNames[39][23]; /* [1..39] of string [22] */
char defaultTeamNames[10][25]; /* [1..10] of string [24] */

static const JE_EditorItemAvailType initialEditorItemAvail =  // FKA initialItemAvail
{
	1,1,1,0,0,1,1,0,1,1,1,1,1,0,1,0,1,1,1,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0, /* Front/Rear Weapons 1-38  */
	0,0,0,0,0,0,0,0,0,0,1,                                                           /* Fill                     */
	1,0,0,0,0,1,0,0,0,1,1,0,1,0,0,0,0,0,                                             /* Sidekicks          51-68 */
	0,0,0,0,0,0,0,0,0,0,0,                                                           /* Fill                     */
	1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                                                   /* Special Weapons    81-93 */
	0,0,0,0,0                                                                        /* Fill                     */
};

/* Last 2 bytes = Word
 *
 * Max Value = 1680
 * X div  60 = Armor  (1-28)
 * X div 168 = Shield (1-12)
 * X div 280 = Engine (1-06)
 */

JE_boolean smoothies[9] = /* [1..9] */
{ 0, 0, 0, 0, 0, 0, 0, 0, 0 };

JE_byte starShowVGASpecialCode;

/* CubeData */
JE_word lastCubeMax, cubeMax;
JE_word cubeList[4]; /* [1..4] */

/* High-Score Stuff */
JE_boolean gameHasRepeated;  // can only get highscore on first play-through

/* Difficulty */
/* Timed Battle */
JE_byte timeBattleSelection;

JE_shortint difficultyLevel, oldDifficultyLevel,
            initialDifficulty;  // can only get highscore on initial episode

/* Player Stuff */
uint    power, lastPower, powerAdd;
JE_byte shieldWait, shieldT;

JE_byte          shotRepeat[11], shotMultiPos[11];
JE_boolean       portConfigChange, portConfigDone;

/* Level Data */
char    lastLevelName[11], levelName[11]; /* string [10] */
JE_byte mainLevel, nextLevel, saveLevel;   /*Current Level #*/

/* Keyboard Junk */
KeySettings keySettings;

/* Mouse settings */
MouseSettings mouseSettings;

int lastSaveSlot = 0;  // save slot last saved or loaded (1-based), 0 = none; for Continue
static const char *const rumbleStrengthCodes[RUMBLE_STRENGTHS] = { "off", "weak", "strong" };
bool autoFireToggle = false;  // fire button toggles constant fire instead of being held

/* Configuration */
JE_shortint levelFilter, levelFilterNew, levelBrightness, levelBrightnessChg;
JE_boolean  filtrationAvail, filterActive, filterFade, filterFadeStart;

JE_boolean gameJustLoaded;

JE_boolean galagaMode;

JE_boolean extraGame;

JE_boolean twoPlayerMode, twoPlayerLinked, onePlayerAction, timedBattleMode, superTyrian;
JE_boolean trentWin = false;
JE_byte    superArcadeMode;

JE_byte    superArcadePowerUp;

JE_real linkGunDirec;
JE_byte inputDevice[2] = { 1, 2 }; // 0:any  1:keyboard  2:mouse  3+:joystick

JE_byte secretHint;
JE_byte background3over;
JE_byte background2over;
JE_byte gammaCorrection;
JE_boolean superPause = false;
JE_boolean explosionTransparent,
           youAreCheating,
           displayScore,
           background2, smoothScroll, wild, superWild, starActive,
           topEnemyOver,
           skyEnemyOverAll,
           background2notTransparent;

JE_byte    fastPlay;
JE_boolean pentiumMode;

/* Savegame files */
JE_byte    gameSpeed;
JE_byte    processorType;  /* 1=386 2=486 3=Pentium Hyper */

JE_SaveFilesType saveFiles; /*array[1..saveLevelnum] of savefiletype;*/

JE_EditorItemAvailType editorItemAvail;

T2KHighScoreType t2kHighScores[20][3];

JE_word editorLevel;   /*Initial value 800*/

Config opentyrian_config;  // implicitly initialized

// Fields of TYRIAN.CFG that are preserved for compatibility
static Uint8 inputDevice_ = 0;  // FKA inputDevice
static Uint8 jConfigure = 0;  // FKA NortSong.jConfigure
static Uint8 midiPort = 0;  // FKA NortSong.midiPort
static Uint8 soundEffects = 0;  // FKA NortSong.soundEffects
static Uint8 versionNum;   /* SW 1.0 and SW/Reg 1.1 = 0 or 1
                            * EA 1.2 = 2        T2K = 3 */

// Don't overwrite the user's files if loading them never happened (e.g. data files are missing).
static bool configurationLoaded = false;
static bool savesLoaded = false;
static const Uint8 defaultJoyButtonAssign[4] = { 1, 4, 5, 5 };  // FKA Joystick.defaultJoyButtonAssign
static Uint8 joyButtonAssign[4] = { 0 };  // FKA Joystick.joyButtonAssign
static Uint8 inputDevice1 = 0;
static Uint8 inputDevice2 = 0;
static const Uint8 defaultDosKeySettings[8] = { 72, 80, 75, 77, 57, 28, 29, 56 };  // FKA defaultKeySettings
static Uint8 dosKeySettings[8] = { 0 };  // FKA keySettings

static const char *const opentyrianConfigFilename = "opentyrian.cfg";

// SteamOS sets SteamDeck=1 for what Steam starts; the board name covers the Desktop Mode terminal
static bool running_on_steam_deck(void)
{
	const char *deck = SDL_getenv("SteamDeck");
	if (deck != NULL && SDL_strcmp(deck, "1") == 0)
		return true;

#if defined(__linux__) && !defined(__EMSCRIPTEN__)
	FILE *f = fopen("/sys/devices/virtual/dmi/id/board_vendor", "r");
	if (f == NULL)
		return false;
	char vendor[16] = "";
	const bool read = fgets(vendor, sizeof(vendor), f) != NULL;
	fclose(f);
	return read && strncmp(vendor, "Valve", 5) == 0;
#else
	return false;
#endif
}

static void loadOpenTyrianConfig(void)
{
	// defaults
	fullscreen_display = -1;
	set_scaler_by_name("None");
	memcpy(keySettings, defaultKeySettings, sizeof(keySettings));
	memcpy(mouseSettings, defaultMouseSettings, sizeof(mouseSettings));
	
	Config *config = &opentyrian_config;

	File file = userFileOpen(opentyrianConfigFilename, "r");
	if (file.error)
	{
		logWarn("Failed to open '%s': %s", opentyrianConfigFilename, fileGetError(&file));

		// First start on a Steam Deck: full screen, and the 4:3 picture with even pixels
		// (1280x800 enlarges 320x200 by 4 down but 3.33 across in Fit 4:3), and
		// smooth motion (the screen runs at 60 or 90 Hz, which 35 does not divide).
		if (running_on_steam_deck())
		{
			logInfo("Steam Deck: starting in full screen with Sharp Bilinear and Smooth Motion.");
			fullscreen_display = 0;
			sharp_bilinear_enabled = true;
			smooth_motion = true;
		}

		return;
	}
	
	bool success = config_parse(config, file.f);
	file.error |= ferror(file.f) != 0;

	if (file.error)
		logError("Failed to read from '%s': %s", opentyrianConfigFilename, fileGetError(&file));
	else if (!success)
		logError("Failed to parse '%s'.", opentyrianConfigFilename);

	fileClose(&file);

	if (!success)
		return;
	
	ConfigSection *section;
	
	section = config_find_section(config, "video", NULL);
	if (section != NULL)
	{
		config_get_int_option(section, "fullscreen", &fullscreen_display);
		
		// Scale2x was the default up to v1.3 and was saved for everyone, so it only counts
		// when the player picked it in a menu; any other saved scaler was a choice.
		config_get_bool_option(section, "scaler_chosen", &scaler_chosen);

		const char *scaler;
		if (config_get_string_option(section, "scaler", &scaler) &&
		    (scaler_chosen || strcmp(scaler, "Scale2x") != 0))
		{
			set_scaler_by_name(scaler);
			scaler_chosen = scaler_chosen || strcmp(scaler, "None") != 0;
		}
		
		const char *scaling_mode;
		if (config_get_string_option(section, "scaling_mode", &scaling_mode))
			set_scaling_mode_by_name(scaling_mode);

		bool vsync;
		if (config_get_bool_option(section, "vsync", &vsync))
			vsync_enabled = vsync;

		bool sharpBilinear;
		if (config_get_bool_option(section, "sharp_bilinear", &sharpBilinear))
			sharp_bilinear_enabled = sharpBilinear;

		config_get_bool_option(section, "smooth_motion", &smooth_motion);

		int w, h;
		if (config_get_int_option(section, "window_width", &w) && config_get_int_option(section, "window_height", &h) &&
		    w > 0 && h > 0 && w <= 16384 && h <= 16384)
		{
			window_geometry.w = w;
			window_geometry.h = h;

			int x, y;
			window_geometry.has_position = config_get_int_option(section, "window_x", &x) && config_get_int_option(section, "window_y", &y);
			if (window_geometry.has_position)
			{
				window_geometry.x = x;
				window_geometry.y = y;
			}

			bool maximized;
			if (config_get_bool_option(section, "window_maximized", &maximized))
				window_geometry.maximized = maximized;
		}
	}

	section = config_find_section(config, "gameplay", NULL);
	if (section != NULL)
	{
		bool autoFire;
		if (config_get_bool_option(section, "auto_fire", &autoFire))
			autoFireToggle = autoFire;

		const char *vibration;
		if (config_get_string_option(section, "vibration", &vibration))
		{
			for (int i = 0; i < RUMBLE_STRENGTHS; ++i)
				if (strcmp(vibration, rumbleStrengthCodes[i]) == 0)
					rumble_strength = i;
		}

		int lastSlot;
		if (config_get_int_option(section, "last_save", &lastSlot) && lastSlot >= 1 && lastSlot <= SAVE_FILES_NUM)
			lastSaveSlot = lastSlot;
	}

	section = config_find_section(config, "sound", NULL);
	if (section != NULL)
	{
		config_get_bool_option(section, "music_chip_chosen", &opl_chosen);

		const char *chip;
		if (opl_chosen && config_get_string_option(section, "music_chip", &chip))
			opl_accurate = strcmp(chip, "classic") != 0;
	}

	section = config_find_section(config, "language", NULL);
	if (section != NULL)
	{
		const char *code;
		if (config_get_string_option(section, "language", &code))
		{
			const int setting = lang_setting_from_code(code);
			if (setting >= 0)
				lang_setting = setting;
		}
	}

	// a player from before What's New had its notice to see
	SDL_strlcpy(whatsnew_seen, "v0", sizeof(whatsnew_seen));

	section = config_find_section(config, "updates", NULL);
	if (section != NULL)
	{
		bool check;
		if (config_get_bool_option(section, "check", &check))
			update_check_enabled = check;

		const char *seen;
		if (config_get_string_option(section, "notes_seen", &seen))
			SDL_strlcpy(whatsnew_seen, seen, sizeof(whatsnew_seen));
	}

	section = config_find_section(config, "keyboard", NULL);
	if (section != NULL)
	{
		for (size_t i = 0; i < COUNTOF(keySettings); ++i)
		{
			const char *keyName;
			if (config_get_string_option(section, keySettingNames[i], &keyName))
			{
				SDL_Scancode scancode = SDL_GetScancodeFromName(keyName);
				if (scancode != SDL_SCANCODE_UNKNOWN)
					keySettings[i] = scancode;
			}
		}
	}

	section = config_find_section(config, "mouse", NULL);
	if (section != NULL)
	{
		for (size_t i = 0; i < COUNTOF(mouseSettings); ++i)
		{
			const char *mouseValue;
			if (config_get_string_option(section, mouseSettingNames[i], &mouseValue))
			{
				for (size_t val = 1; val <= COUNTOF(mouseSettingValues); ++val)
				{
					if (strcmp(mouseValue, mouseSettingValues[val - 1]))
						continue;

					mouseSettings[i] = val;
					break;
				}
			}
		}
	}
}

static void saveOpenTyrianConfig(void)
{
	Config *config = &opentyrian_config;
	
	ConfigSection *section;
	
	section = config_find_or_add_section(config, "video", NULL);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory
	
	config_set_int_option(section, "fullscreen", fullscreen_display);
	
	config_set_string_option(section, "scaler", scalers[scaler].name);
	config_set_bool_option(section, "scaler_chosen", scaler_chosen, NO_YES);
	
	config_set_string_option(section, "scaling_mode", scaling_mode_names[scaling_mode]);

	config_set_bool_option(section, "vsync", vsync_enabled, OFF_ON);

	config_set_bool_option(section, "sharp_bilinear", sharp_bilinear_enabled, OFF_ON);

	config_set_bool_option(section, "smooth_motion", smooth_motion, OFF_ON);

	if (window_geometry.w > 0)
	{
		config_set_int_option(section, "window_width", window_geometry.w);
		config_set_int_option(section, "window_height", window_geometry.h);
		if (window_geometry.has_position)
		{
			config_set_int_option(section, "window_x", window_geometry.x);
			config_set_int_option(section, "window_y", window_geometry.y);
		}
		config_set_bool_option(section, "window_maximized", window_geometry.maximized, NO_YES);
	}

	section = config_find_or_add_section(config, "gameplay", NULL);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	config_set_bool_option(section, "auto_fire", autoFireToggle, OFF_ON);
	config_set_string_option(section, "vibration", rumbleStrengthCodes[rumble_strength]);
	if (lastSaveSlot > 0)
		config_set_int_option(section, "last_save", lastSaveSlot);

	section = config_find_or_add_section(config, "sound", NULL);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	config_set_string_option(section, "music_chip", opl_accurate ? "accurate" : "classic");
	config_set_bool_option(section, "music_chip_chosen", opl_chosen, NO_YES);

	section = config_find_or_add_section(config, "language", NULL);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	config_set_string_option(section, "language", lang_setting_code(lang_setting));

	section = config_find_or_add_section(config, "updates", NULL);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	config_set_bool_option(section, "check", update_check_enabled, OFF_ON);
	if (whatsnew_seen[0] != '\0')
		config_set_string_option(section, "notes_seen", whatsnew_seen);

	section = config_find_or_add_section(config, "keyboard", NULL);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	for (size_t i = 0; i < COUNTOF(keySettings); ++i)
	{
		const char *keyName = SDL_GetScancodeName(keySettings[i]);
		if (keyName[0] == '\0')
			keyName = NULL;
		config_set_string_option(section, keySettingNames[i], keyName);
	}

	// Tyrian 2000 doesn't save mouse settings, so we do it ourselves
	section = config_find_or_add_section(config, "mouse", NULL);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	for (size_t i = 0; i < COUNTOF(mouseSettings); ++i)
		config_set_string_option(section, mouseSettingNames[i], mouseSettingValues[mouseSettings[i] - 1]);

	File file = userFileOpen(opentyrianConfigFilename, "w");
	if (file.error)
	{
		logError("Failed to open '%s': %s", opentyrianConfigFilename, fileGetError(&file));

		return;
	}
	
	config_write(config, file.f);
	file.error |= ferror(file.f) != 0;

	fileFlush(&file);

	if (file.error)
		logError("Failed to write to '%s': %s", opentyrianConfigFilename, fileGetError(&file));

	fileClose(&file);
}

static void playeritems_to_pitems(JE_PItemsType pItems, PlayerItems *items, JE_byte initial_episode_num)
{
	pItems[0]  = items->weapon[FRONT_WEAPON].id;
	pItems[1]  = items->weapon[REAR_WEAPON].id;
	pItems[2]  = items->super_arcade_mode;
	pItems[3]  = items->sidekick[LEFT_SIDEKICK];
	pItems[4]  = items->sidekick[RIGHT_SIDEKICK];
	pItems[5]  = items->generator;
	pItems[6]  = items->sidekick_level;
	pItems[7]  = items->sidekick_series;
	pItems[8]  = initial_episode_num;
	pItems[9]  = items->shield;
	pItems[10] = items->special;
	pItems[11] = items->ship;
}

static void pitems_to_playeritems(PlayerItems *items, JE_PItemsType pItems, JE_byte *initial_episode_num)
{
	items->weapon[FRONT_WEAPON].id  = pItems[0];
	items->weapon[REAR_WEAPON].id   = pItems[1];
	items->super_arcade_mode        = pItems[2];
	items->sidekick[LEFT_SIDEKICK]  = pItems[3];
	items->sidekick[RIGHT_SIDEKICK] = pItems[4];
	items->generator                = pItems[5];
	items->sidekick_level           = pItems[6];
	items->sidekick_series          = pItems[7];
	if (initial_episode_num != NULL)
		*initial_episode_num        = pItems[8];
	items->shield                   = pItems[9];
	items->special                  = pItems[10];
	items->ship                     = pItems[11];
}

void JE_saveGame(JE_byte slot, const char *name)
{
	assert(strlen(name) >= 14);

	const bool lastSaveSlotChanged = lastSaveSlot != slot;
	lastSaveSlot = slot;

	saveFiles[slot-1].initialDifficulty = initialDifficulty;
	saveFiles[slot-1].gameHasRepeated = gameHasRepeated;
	saveFiles[slot-1].level = saveLevel;
	
	if (superTyrian)
		player[0].items.super_arcade_mode = SA_SUPERTYRIAN;
	else if (superArcadeMode == SA_NONE && onePlayerAction)
		player[0].items.super_arcade_mode = SA_ARCADE;
	else
		player[0].items.super_arcade_mode = superArcadeMode;
	
	playeritems_to_pitems(saveFiles[slot-1].items, &player[0].items, initial_episode_num);
	
	if (twoPlayerMode)
		playeritems_to_pitems(saveFiles[slot-1].lastItems, &player[1].items, 0);
	else
		playeritems_to_pitems(saveFiles[slot-1].lastItems, &player[0].last_items, 0);
	
	saveFiles[slot-1].score  = player[0].cash;
	saveFiles[slot-1].score2 = player[1].cash;
	
	memcpy(&saveFiles[slot-1].levelName, &lastLevelName, sizeof(lastLevelName));
	saveFiles[slot-1].cubes  = lastCubeMax;

	if (strcmp(lastLevelName, "Completed") == 0)
	{
		temp = episodeNum - 1;
		if (temp < 1)
		{
			temp = EPISODE_AVAILABLE; /* JE: {Episodemax is 4 for completion purposes} */
		}
		saveFiles[slot-1].episode = temp;
	}
	else
	{
		saveFiles[slot-1].episode = episodeNum;
	}

	saveFiles[slot-1].difficulty = difficultyLevel;
	saveFiles[slot-1].secretHint = secretHint;
	saveFiles[slot-1].input1 = inputDevice[0];
	saveFiles[slot-1].input2 = inputDevice[1];

	memcpy(saveFiles[slot-1].name, name, 14);
	saveFiles[slot-1].name[14] = '\0';
	
	for (uint port = 0; port < 2; ++port)
	{
		// if two-player, use first player's front and second player's rear weapon
		saveFiles[slot-1].power[port] = player[twoPlayerMode ? port : 0].items.weapon[port].power;
	}
	
	saveSaves();

	// keep Continue pointing at this save even if the game does not close normally
	if (lastSaveSlotChanged)
		saveOpenTyrianConfig();
}

void JE_loadGame(JE_byte slot)
{
	superTyrian = false;
	onePlayerAction = false;
	twoPlayerMode = false;
	extraGame = false;
	galagaMode = false;
	timedBattleMode = false;

	lastSaveSlot = slot;

	initialDifficulty = saveFiles[slot-1].initialDifficulty;
	gameHasRepeated   = saveFiles[slot-1].gameHasRepeated;
	twoPlayerMode     = (slot-1) > 10;
	difficultyLevel   = saveFiles[slot-1].difficulty;
	
	pitems_to_playeritems(&player[0].items, saveFiles[slot-1].items, &initial_episode_num);
	
	superArcadeMode = player[0].items.super_arcade_mode;
	
	if (superArcadeMode == SA_SUPERTYRIAN)
		superTyrian = true;
	if (superArcadeMode != SA_NONE)
		onePlayerAction = true;
	if (superArcadeMode > SA_LASTSHIP)
		superArcadeMode = SA_NONE;
	
	if (twoPlayerMode)
	{
		onePlayerAction = false;
		
		pitems_to_playeritems(&player[1].items, saveFiles[slot-1].lastItems, NULL);
	}
	else
	{
		pitems_to_playeritems(&player[0].last_items, saveFiles[slot-1].lastItems, NULL);
	}

	/* Compatibility with old version */
	if (player[1].items.sidekick_level < 101)
	{
		player[1].items.sidekick_level = 101;
		player[1].items.sidekick_series = player[1].items.sidekick[LEFT_SIDEKICK];
	}
	
	player[0].cash = saveFiles[slot-1].score;
	player[1].cash = saveFiles[slot-1].score2;
	
	mainLevel   = saveFiles[slot-1].level;
	cubeMax     = saveFiles[slot-1].cubes;
	lastCubeMax = cubeMax;

	secretHint = saveFiles[slot-1].secretHint;
	inputDevice[0] = saveFiles[slot-1].input1;
	inputDevice[1] = saveFiles[slot-1].input2;

	for (uint port = 0; port < 2; ++port)
	{
		// if two-player, use first player's front and second player's rear weapon
		player[twoPlayerMode ? port : 0].items.weapon[port].power = saveFiles[slot-1].power[port];
	}
	
	int episode = saveFiles[slot-1].episode;

	memcpy(&levelName, &saveFiles[slot-1].levelName, sizeof(levelName));

	if (strcmp(levelName, "Completed") == 0)
	{
		if (episode == EPISODE_AVAILABLE)
			episode = 1;
		else if (episode < EPISODE_AVAILABLE)
			episode++;
		/* Increment episode.  Episode EPISODE_AVAILABLE goes to 1. */
	}

	JE_initEpisode(episode);
	saveLevel = mainLevel;
	memcpy(&lastLevelName, &levelName, sizeof(levelName));
}

void JE_initProcessorType(void)
{
	/* SYN: Originally this proc looked at your hardware specs and chose appropriate options. We don't care, so I'll just set
	   decent defaults here. */

	wild = false;
	superWild = false;
	smoothScroll = true;
	explosionTransparent = true;
	filtrationAvail = false;
	background2 = true;
	displayScore = true;

	switch (processorType)
	{
		case 1: /* 386 */
			background2 = false;
			displayScore = false;
			explosionTransparent = false;
			break;
		case 2: /* 486 - Default */
			break;
		case 3: /* High Detail */
			smoothScroll = false;
			break;
		case 4: /* Pentium */
			wild = true;
			filtrationAvail = true;
			break;
		case 5: /* Nonstandard VGA */
			smoothScroll = false;
			break;
		case 6: /* SuperWild */
			wild = true;
			superWild = true;
			filtrationAvail = true;
			break;
	}

	switch (gameSpeed)
	{
		case 1:  /* Slug Mode */
			fastPlay = 3;
			break;
		case 2:  /* Slower */
			fastPlay = 4;
			break;
		case 3: /* Slow */
			fastPlay = 5;
			break;
		case 4: /* Normal */
			fastPlay = 0;
			break;
		case 5: /* Pentium Hyper */
			fastPlay = 1;
			break;
	}

}

void JE_setNewGameSpeed(void)
{
	pentiumMode = false;

	Uint16 speed;
	switch (fastPlay)
	{
	default:
		assert(false);
		// fall through
	case 0:  // Normal
		speed = 0x4300;
		smoothScroll = true;
		frameCountMax = 2;
		break;
	case 1:  // Pentium Hyper
		speed = 0x3000;
		smoothScroll = true;
		frameCountMax = 2;
		break;
	case 2:
		speed = 0x2000;
		smoothScroll = false;
		frameCountMax = 2;
		break;
	case 3:  // Slug mode
		speed = 0x5300;
		smoothScroll = true;
		frameCountMax = 4;
		break;
	case 4:  // Slower
		speed = 0x4300;
		smoothScroll = true;
		frameCountMax = 3;
		break;
	case 5:  // Slow
		speed = 0x4300;
		smoothScroll = true;
		frameCountMax = 2;
		pentiumMode = true;
		break;
	}

	setFrameSpeed(speed);
	setFrameCount(frameCountMax);
}

static const char *const tyrianConfigFilename = "tyrian.cfg";

void loadConfiguration(void)
{
	bool invalid = false;

	File file = userFileOpen(tyrianConfigFilename, "rb");
	if (file.error)
	{
		logWarn("Failed to open '%s': %s", tyrianConfigFilename, fileGetError(&file));

		invalid = true;
	}
	else
	{
		Uint8 data[28];
		fileReadExactly(&file, data, sizeof data);

		invalid |= fileGetLength(&file) != sizeof data;

		invalid |= file.error;

		if (file.error)
			logError("Failed to read from '%s': %s", tyrianConfigFilename, fileGetError(&file));

		fileClose(&file);

		MemReader reader = { data, sizeof data, false };

		background2     = memReadBool(&reader);
		gameSpeed       = memReadU8(&reader);
		inputDevice_    = memReadU8(&reader);
		jConfigure      = memReadU8(&reader);
		versionNum      = memReadU8(&reader);
		processorType   = memReadU8(&reader);
		midiPort        = memReadU8(&reader);
		soundEffects    = memReadU8(&reader);
		gammaCorrection = memReadU8(&reader);
		difficultyLevel = memReadS8(&reader);
		memReadU8Array(&reader, joyButtonAssign, COUNTOF(joyButtonAssign));
		tyrMusicVolume  = memReadU16(&reader);
		fxVolume        = memReadU16(&reader);
		inputDevice1    = memReadU8(&reader);
		inputDevice2    = memReadU8(&reader);
		memReadU8Array(&reader, dosKeySettings, COUNTOF(dosKeySettings));

		assert(reader.size == 0 || reader.error);
		invalid |= reader.error;

		inputDevice_ = 0;
		if (jConfigure == 0)
			jConfigure = 1;
		// Game resets version number; ShipEdit doesn't.
		versionNum = 3;

		if (tyrMusicVolume > 255)
			tyrMusicVolume = 255;
		if (fxVolume > 255)
			fxVolume = 255;
	}

	if (invalid)
	{
		logWarn("'%s' is invalid or missing.", tyrianConfigFilename);
		
		background2 = true;
		gameSpeed = 4;
		inputDevice_ = 0;
		jConfigure = 0;
		versionNum = 3;
		processorType = 3;
		midiPort = 1;
		soundEffects = 1;
		gammaCorrection = 0;
		difficultyLevel = 0;
		memcpy(&joyButtonAssign, &defaultJoyButtonAssign, sizeof(joyButtonAssign));
		tyrMusicVolume = 223;
		fxVolume = 223;
		inputDevice1 = 0;
		inputDevice2 = 0;
		memcpy(&dosKeySettings, &defaultDosKeySettings, sizeof(dosKeySettings));
	}
	
	loadOpenTyrianConfig();

	set_volume(tyrMusicVolume, fxVolume);

	JE_initProcessorType();

	configurationLoaded = true;
}

void saveConfiguration(void)
{
	if (!configurationLoaded)
		return;

	Uint8 data[28];

	MemWriter writer = { data, sizeof data, false };

	memWriteBool(&writer,    background2);
	memWriteU8(&writer,      gameSpeed);
	memWriteU8(&writer,      inputDevice_);
	memWriteU8(&writer,      jConfigure);
	memWriteU8(&writer,      versionNum);
	memWriteU8(&writer,      processorType);
	memWriteU8(&writer,      midiPort);
	memWriteU8(&writer,      soundEffects);
	memWriteU8(&writer,      gammaCorrection);
	memWriteS8(&writer,      difficultyLevel);
	memWriteU8Array(&writer, joyButtonAssign, COUNTOF(joyButtonAssign));
	memWriteU16(&writer,     tyrMusicVolume);
	memWriteU16(&writer,     fxVolume);
	memWriteU8(&writer,      inputDevice1);
	memWriteU8(&writer,      inputDevice2);
	memWriteU8Array(&writer, dosKeySettings, COUNTOF(dosKeySettings));

	assert(writer.size == 0 && !writer.error);

	File file = userFileOpen(tyrianConfigFilename, "wb");
	if (file.error)
	{
		logError("Failed to open '%s': %s", tyrianConfigFilename, fileGetError(&file));
	}
	else
	{
		fileWrite(&file, data, sizeof data);
		fileFlush(&file);

		if (file.error)
			logError("Failed to write to '%s': %s", tyrianConfigFilename, fileGetError(&file));

		fileClose(&file);
	}

	saveOpenTyrianConfig();
}

static const char *const tyrianSaveFilename = "tyrian.sav";

/* Tyrian 2000 appends its own high score tables to TYRIAN.SAV, unencrypted, after the regular save data.
 * Tables 0-9 are Timed Battle and tables 10-19 are the episodes (which have an extra unknown field). */
enum
{
	T2K_HIGH_SCORE_NAME_SIZE = 29,
	T2K_TIMED_HIGH_SCORE_SIZE = 4 + 1 + T2K_HIGH_SCORE_NAME_SIZE + 1,
	T2K_EPISODE_HIGH_SCORE_SIZE = 4 + 4 + 1 + T2K_HIGH_SCORE_NAME_SIZE + 1,
	T2K_HIGH_SCORES_SIZE = 10 * 3 * T2K_TIMED_HIGH_SCORE_SIZE + 10 * 3 * T2K_EPISODE_HIGH_SCORE_SIZE,
};

static void resetT2KHighScores(void)
{
	for (size_t z = 0; z < 10; ++z)
	{
		for (size_t y = 0; y < 3; ++y)
		{
			// Timed Battle scores
			t2kHighScores[z][y].score = ((mt_rand() % 50) + 1) * 100;
			strcpy(t2kHighScores[z][y].playerName, defaultHighScoreNames[mt_rand() % COUNTOF(defaultHighScoreNames)]);
			t2kHighScores[z][y].difficulty = 0;
		}
	}
	for (size_t z = 10; z < 20; ++z)
	{
		for (size_t y = 0; y < 3; ++y)
		{
			// Main Game scores
			t2kHighScores[z][y].score = ((mt_rand() % 20) + 1) * 1000;
			if (z & 1)
				strcpy(t2kHighScores[z][y].playerName, defaultTeamNames[mt_rand() % COUNTOF(defaultTeamNames)]);
			else
				strcpy(t2kHighScores[z][y].playerName, defaultHighScoreNames[mt_rand() % COUNTOF(defaultHighScoreNames)]);
			t2kHighScores[z][y].difficulty = 0;
		}
	}
}

static bool readT2KHighScores(MemReader *reader)
{
	for (size_t z = 0; z < 20; ++z)
	{
		for (size_t y = 0; y < 3; ++y)
		{
			T2KHighScoreType *highScore = &t2kHighScores[z][y];

			highScore->score = memReadS32(reader);
			if (z >= 10)
				(void)memReadU32(reader);  // Unknown long int that seems to have no effect
			Uint8 nameLen = memReadU8(reader);
			memReadCharArray(reader, highScore->playerName, T2K_HIGH_SCORE_NAME_SIZE);
			highScore->playerName[MIN(nameLen, T2K_HIGH_SCORE_NAME_SIZE)] = '\0';
			highScore->difficulty = memReadU8(reader);
		}
	}

	return !reader->error;
}

static void writeT2KHighScores(MemWriter *writer)
{
	for (size_t z = 0; z < 20; ++z)
	{
		for (size_t y = 0; y < 3; ++y)
		{
			const T2KHighScoreType *highScore = &t2kHighScores[z][y];

			memWriteS32(writer, highScore->score);
			if (z >= 10)
				memWriteU32(writer, 0x12345678);  // Unknown long int that seems to have no effect
			memWriteU8(writer, MIN(strlen(highScore->playerName), T2K_HIGH_SCORE_NAME_SIZE));
			memWriteCharArray(writer, highScore->playerName, T2K_HIGH_SCORE_NAME_SIZE);
			memWriteU8(writer, highScore->difficulty);
		}
	}
}

static bool decryptSaveData(Uint8 *data);

// copies an unreadable tyrian.sav to tyrian.sav.bak before it is replaced
static void backUpInvalidSaves(void)
{
	static const char *const backupFilename = "tyrian.sav.bak";

	File file = userFileOpen(tyrianSaveFilename, "rb");
	if (file.error)
		return;

	static Uint8 data[64 * 1024];  // tyrian.sav is about 4 KB
	const size_t size = fileReadAtMost(&file, data, sizeof data);
	fileClose(&file);
	if (size == 0)
		return;

	File backup = userFileOpen(backupFilename, "wb");
	if (!backup.error)
	{
		fileWrite(&backup, data, size);
		fileFlush(&backup);
	}

	if (backup.error)
		logError("Failed to back up '%s' to '%s': %s", tyrianSaveFilename, backupFilename, fileGetError(&backup));
	else
		logWarn("Kept a copy of the unreadable '%s' as '%s'.", tyrianSaveFilename, backupFilename);

	fileClose(&backup);
}

// What tyrian.sav held when the game last read or wrote it.  If it changes
// on its own, someone put another one there (saves brought from another
// computer or from the browser): it is read again at the title screen and
// not overwritten when the game closes.
static bool savesFileKnown = false;
static Uint32 savesFileHash;

static bool hashSavesFile(Uint32 *out_hash)
{
	File file = userFileOpen(tyrianSaveFilename, "rb");
	if (file.error)
		return false;

	static Uint8 data[64 * 1024];  // tyrian.sav is about 4 KB
	const size_t size = fileReadAtMost(&file, data, sizeof data);
	const bool ok = !file.error;
	fileClose(&file);

	Uint32 hash = 2166136261u;  // FNV-1a
	for (size_t i = 0; i < size; ++i)
		hash = (hash ^ data[i]) * 16777619u;
	*out_hash = hash ^ (Uint32)size;
	return ok;
}

static bool savesReplaced(void)
{
	Uint32 hash;
	return savesFileKnown && hashSavesFile(&hash) && hash != savesFileHash;
}

void reloadSavesIfReplaced(void)
{
	if (!savesLoaded || !savesReplaced())
		return;

	logInfo("'%s' was replaced; loading it again.", tyrianSaveFilename);
	loadSaves();
}

void saveSavesUnlessReplaced(void)
{
	if (savesReplaced())
		logInfo("'%s' was replaced; keeping the new one.", tyrianSaveFilename);
	else
		saveSaves();
}

void loadSaves(void)
{
	bool invalid = false;

	File file = userFileOpen(tyrianSaveFilename, "rb");
	if (file.error)
	{
		logWarn("Failed to open '%s': %s", tyrianSaveFilename, fileGetError(&file));

		invalid = true;
	}
	else
	{
		Uint8 data[SAVE_FILE_SIZE + 4];
		fileReadExactly(&file, data, sizeof data);

		Uint8 highScoreData[T2K_HIGH_SCORES_SIZE];
		size_t highScoreDataSize = file.error ? 0 : fileReadAtMost(&file, highScoreData, sizeof highScoreData);

		invalid |= file.error;

		if (file.error)
			logError("Failed to read from '%s': %s", tyrianSaveFilename, fileGetError(&file));

		fileClose(&file);

		MemReader highScoreReader = { highScoreData, highScoreDataSize, false };
		if (invalid || !readT2KHighScores(&highScoreReader))
		{
			if (!invalid)
				logWarn("'%s' has no Tyrian 2000 high scores; using defaults.", tyrianSaveFilename);
			resetT2KHighScores();
		}

		invalid |= !decryptSaveData(data);

		MemReader reader = { data, sizeof data, false };

		for (size_t i = 0; i < COUNTOF(saveFiles); ++i)
		{
			saveFiles[i].encode            = memReadU16(&reader);
			saveFiles[i].level             = memReadU16(&reader);
			memReadU8Array(&reader, saveFiles[i].items, COUNTOF(saveFiles[i].items));
			saveFiles[i].score             = memReadU32(&reader);
			saveFiles[i].score2            = memReadU32(&reader);
			Uint8 levelNameLen             = memReadU8(&reader);
			memReadCharArray(&reader, saveFiles[i].levelName, 9);
			saveFiles[i].levelName[MIN(levelNameLen, 9)] = '\0';
			memReadCharArray(&reader, saveFiles[i].name, 14);
			saveFiles[i].name[14] = '\0';
			saveFiles[i].cubes             = memReadU8(&reader);
			memReadU8Array(&reader, saveFiles[i].power, COUNTOF(saveFiles[i].power));
			saveFiles[i].episode           = memReadU8(&reader);
			memReadU8Array(&reader, saveFiles[i].lastItems, COUNTOF(saveFiles[i].lastItems));
			saveFiles[i].difficulty        = memReadU8(&reader);
			saveFiles[i].secretHint        = memReadU8(&reader);
			saveFiles[i].input1            = memReadU8(&reader);
			saveFiles[i].input2            = memReadU8(&reader);
			saveFiles[i].gameHasRepeated   = memReadBool(&reader);
			saveFiles[i].initialDifficulty = memReadU8(&reader);
			saveFiles[i].highScore1        = memReadS32(&reader);
			saveFiles[i].highScore2        = memReadS32(&reader);
			Uint8 highScoreNameLen         = memReadU8(&reader);
			memReadCharArray(&reader, saveFiles[i].highScoreName, 29);
			saveFiles[i].highScoreName[MIN(highScoreNameLen, 29)] = '\0';
			saveFiles[i].highScoreDiff     = memReadU8(&reader);
		}

		memReadU8Array(&reader, editorItemAvail, COUNTOF(editorItemAvail));

		editorLevel = ((Uint16)editorItemAvail[98] << 8) | editorItemAvail[99];

		assert(reader.size == 4 || reader.error);
		invalid |= reader.error;
	}

	if (invalid)
	{
		logWarn("'%s' is invalid or missing.", tyrianSaveFilename);

		if (userFileExists(tyrianSaveFilename))
			backUpInvalidSaves();

		memset(saveFiles, 0, sizeof(saveFiles));

		for (size_t i = 0; i < SAVE_FILES_NUM; ++i)
		{
			saveFiles[i].level = 0;

			for (size_t j = 0; j < 14; ++j)
				saveFiles[i].name[j] = ' ';
			saveFiles[i].name[14] = '\0';

			saveFiles[i].highScore1 = ((mt_rand() % 20) + 1) * 1000;
			if (i % 6 < 3)
			{
				saveFiles[i].highScore2 = 0;
				strcpy(saveFiles[i].highScoreName, defaultHighScoreNames[mt_rand() % COUNTOF(defaultHighScoreNames)]);
			}
			else
			{
				saveFiles[i].highScore2 = ((mt_rand() % 20) + 1) * 1000;
				strcpy(saveFiles[i].highScoreName, defaultTeamNames[mt_rand() % COUNTOF(defaultTeamNames)]);
			}
			saveFiles[i].highScoreDiff = 0;
		}

		memcpy(editorItemAvail, initialEditorItemAvail, sizeof(editorItemAvail));

		editorLevel = 800;

		resetT2KHighScores();
	}

	savesLoaded = true;
	savesFileKnown = !invalid && hashSavesFile(&savesFileHash);
}

static void encryptSaveData(Uint8 *data);

void saveSaves(void)
{
	if (!savesLoaded)
		return;

	Uint8 data[SAVE_FILE_SIZE + 4];

	MemWriter writer = { data, sizeof data, false };

	for (size_t i = 0; i < COUNTOF(saveFiles); ++i)
	{
		memWriteU16(&writer,       saveFiles[i].encode);
		memWriteU16(&writer,       saveFiles[i].level);
		memWriteU8Array(&writer,   saveFiles[i].items, COUNTOF(saveFiles[i].items));
		memWriteU32(&writer,       saveFiles[i].score);
		memWriteU32(&writer,       saveFiles[i].score2);
		memWriteU8(&writer,        strlen(saveFiles[i].levelName));
		memWriteCharArray(&writer, saveFiles[i].levelName, 9);
		memWriteCharArray(&writer, saveFiles[i].name, 14);
		memWriteU8(&writer,        saveFiles[i].cubes);
		memWriteU8Array(&writer,   saveFiles[i].power, COUNTOF(saveFiles[i].power));
		memWriteU8(&writer,        saveFiles[i].episode);
		memWriteU8Array(&writer,   saveFiles[i].lastItems, COUNTOF(saveFiles[i].lastItems));
		memWriteU8(&writer,        saveFiles[i].difficulty);
		memWriteU8(&writer,        saveFiles[i].secretHint);
		memWriteU8(&writer,        saveFiles[i].input1);
		memWriteU8(&writer,        saveFiles[i].input2);
		memWriteBool(&writer,      saveFiles[i].gameHasRepeated);
		memWriteU8(&writer,        saveFiles[i].initialDifficulty);
		memWriteS32(&writer,       saveFiles[i].highScore1);
		memWriteS32(&writer,       saveFiles[i].highScore2);
		memWriteU8(&writer,        strlen(saveFiles[i].highScoreName));
		memWriteCharArray(&writer, saveFiles[i].highScoreName, 29);
		memWriteU8(&writer,        saveFiles[i].highScoreDiff);
	}

	editorItemAvail[98] = editorLevel >> 8;
	editorItemAvail[99] = editorLevel;

	memWriteU8Array(&writer, editorItemAvail, COUNTOF(editorItemAvail));

	assert(writer.size == 4 && !writer.error);

	encryptSaveData(data);

	Uint8 highScoreData[T2K_HIGH_SCORES_SIZE];
	MemWriter highScoreWriter = { highScoreData, sizeof highScoreData, false };
	writeT2KHighScores(&highScoreWriter);
	assert(highScoreWriter.size == 0 && !highScoreWriter.error);

	File file = userFileOpen(tyrianSaveFilename, "wb");
	if (file.error)
	{
		logError("Failed to open '%s': %s", tyrianSaveFilename, fileGetError(&file));
	}
	else
	{
		fileWrite(&file, data, sizeof data);
		fileWrite(&file, highScoreData, sizeof highScoreData);
		fileFlush(&file);

		if (file.error)
			logError("Failed to write to '%s': %s", tyrianSaveFilename, fileGetError(&file));

		fileClose(&file);
	}

	savesFileKnown = hashSavesFile(&savesFileHash);
}

void encryptSaveData(Uint8 *data)
{
	Uint8 y;

	y = 0;
	for (size_t i = 0; i < SAVE_FILE_SIZE; ++i)
		y += data[i];
	data[SAVE_FILE_SIZE] = y;

	y = 0;
	for (size_t i = 0; i < SAVE_FILE_SIZE; ++i)
		y -= data[i];
	data[SAVE_FILE_SIZE + 1] = y;

	y = 1;
	for (size_t i = 0; i < SAVE_FILE_SIZE; ++i)
		y = y * data[i] + 1;
	data[SAVE_FILE_SIZE + 2] = y;

	y = 0;
	for (size_t i = 0; i < SAVE_FILE_SIZE; ++i)
		y ^= data[i];
	data[SAVE_FILE_SIZE + 3] = y;

	for (size_t i = 0; i < SAVE_FILE_SIZE; ++i)
	{
		data[i] ^= cryptKey[(i + 1) % 10];
		if (i > 0)
			data[i] ^= data[i - 1];
	}
}

bool decryptSaveData(Uint8 *data)
{
	for (size_t i = SAVE_FILE_SIZE - 1; ; --i)
	{
		data[i] ^= cryptKey[(i + 1) % 10];
		if (i > 0)
			data[i] ^= data[i - 1];
		else
			break;
	}

	Uint8 y;

	y = 0;
	for (size_t i = 0; i < SAVE_FILE_SIZE; ++i)
		y += data[i];
	if (data[SAVE_FILE_SIZE] != y)
		return false;

	y = 0;
	for (size_t i = 0; i < SAVE_FILE_SIZE; ++i)
		y -= data[i];
	if (data[SAVE_FILE_SIZE + 1] != y)
		return false;

	y = 1;
	for (size_t i = 0; i < SAVE_FILE_SIZE; ++i)
		y = y * data[i] + 1;
	if (data[SAVE_FILE_SIZE + 2] != y)
		return false;

	y = 0;
	for (size_t i = 0; i < SAVE_FILE_SIZE; ++i)
		y ^= data[i];
	if (data[SAVE_FILE_SIZE + 3] != y)
		return false;

	return true;
}
