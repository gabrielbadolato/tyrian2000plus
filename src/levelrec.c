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
 * Level recording: every level is recorded as it is played, in
 * lastlevel.rec in the user folder, so that
 *
 *  - when the game closes in the middle of a level (the window closed, the
 *    Steam Deck turned off, a crash), Continue plays the recording again,
 *    unseen and fast, up to where it stopped, and the level goes on from
 *    there (paused);
 *  - Replay, on the title screen, shows the last level played again.
 *
 * The game is deterministic, as its network mode already needs: the same
 * state at the start of a level, the same random numbers and the same input
 * in every frame give the same level.  So the recording keeps the state at
 * the start (the "LAST LEVEL" save made when the level starts, the two
 * players and the detail settings), the seed of the random numbers, and then,
 * frame by frame, the input of each player after the keyboard, controller,
 * mouse and Auto Fire were combined.  Once a second it also keeps a checksum
 * of the game state; a replay that comes out different stops there.
 *
 * The file: a Header, then a list of tokens (see Token).  It is written as the
 * level goes, so it is there even if the game does not close normally.
 */
#include "levelrec.h"

#include "backgrnd.h"
#include "config.h"
#include "demo.h"
#include "episodes.h"
#include "file.h"
#include "fonthand.h"
#include "lang.h"
#include "logging.h"
#include "loudness.h"
#include "mainint.h"
#include "mtrand.h"
#include "network.h"
#include "player.h"
#include "shots.h"
#include "sprite.h"
#include "varz.h"
#include "video.h"

#include <stdlib.h>
#include <string.h>

const char *const levelrec_filename = "lastlevel.rec";

#define FORMAT_VERSION 1
#define CHECK_INTERVAL 35  // frames: once a second

typedef enum
{
	TOKEN_CHECK = 1,     // + Uint32 checksum, at the top of every CHECK_INTERVAL-th frame
	TOKEN_SETTINGS = 2,  // + processorType, gameSpeed: changed in the pause menu
	TOKEN_SKIP = 3,      // the frame jumped back to its top (after F1 help)
	TOKEN_END = 4,       // the level ended (or a cheat changed it): not resumable
	TOKEN_INPUT = 0x10,  // | player, + the input (INPUT_SIZE bytes)
	TOKEN_REPEAT = 0x20, // | player: the same input as that player's last one
} Token;

#define INPUT_SIZE 13

// What has to be the same when the level is played again, and what the level
// takes from the game around it.
typedef struct
{
	// the same level, in the same kind of game
	Uint8 episode, lvlFileNum, mainLevel;
	char levelName[11];
	bool twoPlayerMode, galagaMode, onePlayerAction, superTyrian, extraGame;
	bool bonusLevelCurrent, normalBonusLevelCurrent;
	Uint8 superArcadeMode;

	// set from the recording (a loaded game does not have all of it)
	Player player[2];
	// Episode 5 plays a step easier, and its "LAST LEVEL" save keeps that
	// easier step (so a game loaded there is easier still).
	Sint8 difficulty, oldDifficulty;
	Uint8 cubeMax;
	bool youAreCheating;
	Uint8 processorType, gameSpeed, fastPlay;
	bool background2, smoothScroll, wild, superWild, explosionTransparent, filtrationAvail, displayScore, pentiumMode;
	bool portConfigDone;  // the rear mode button was let go
	// what a special weapon used at the end of the last level leaves behind
	JE_word flareDuration, specialWeaponWpn;
	bool flareStart, spraySpecial, infiniteShot, linkToPlayer, optionAttachmentReturn;
	Sint8 flareColChg, specialWeaponFilter, specialWeaponFreq, levelFilterNew;
	Uint8 doIced;
	Sint16 explosionFollowAmountX, explosionFollowAmountY;
} StartState;

typedef struct
{
	char magic[8];
	Uint16 version;
	Uint16 headerSize, playerSize, saveSize;
	Uint8 slot;    // the "LAST LEVEL" save: 11, or 22 in two-player games
	Uint32 seed;
	JE_SaveFileType save;
	StartState start;
} Header;

static const char magic[8] = "T2K+REC";

typedef enum
{
	MODE_OFF,
	MODE_RECORD,  // recording the level being played
	MODE_FAST,    // replaying unseen up to where the game closed, then recording
	MODE_WATCH,   // Replay
} Mode;

static Mode mode = MODE_OFF;
static bool resumeRequested, watchRequested;
static bool watchLevel;   // this level is a Replay (until it ends)
static bool headerPending;

static Header header;
static Uint8 *buf;        // the whole file: the header, then the tokens
static size_t len, cap;
static size_t pos;        // replaying: the next token
static size_t written;    // recording: what is in the file already
static File file = { 0 };

static bool loaded;       // buf holds the file as read (see loadRecording)
static bool loadedEnded;  // it has TOKEN_END
static unsigned int loadedChecks;

static unsigned int frame;
static LevelRecInput lastInput[2];
static bool haveLastInput[2];
static Uint8 lastProcessorType, lastGameSpeed;

// the player's own settings, back when the fast replay is over
static Uint8 userProcessorType, userGameSpeed;
static bool userSamplesDisabled;
static Uint32 lastShownTicks;

static Uint32 cheatHash;
static bool failed;
static bool pausePending;  // after a resumed level's first frame is shown

// --- the buffer ---

static void reserve(size_t more)
{
	if (len + more <= cap)
		return;

	size_t newCap = cap ? cap * 2 : 64 * 1024;
	while (newCap < len + more)
		newCap *= 2;

	Uint8 *newBuf = realloc(buf, newCap);
	if (newBuf == NULL)
	{
		logFatal("Out of memory for the level recording.");
		exit(EXIT_FAILURE);
	}
	buf = newBuf;
	cap = newCap;
}

static void putU8(Uint8 value)
{
	reserve(1);
	buf[len++] = value;
}

static void putU16(Uint16 value)
{
	putU8(value & 0xff);
	putU8(value >> 8);
}

static void putU32(Uint32 value)
{
	putU16(value & 0xffff);
	putU16(value >> 16);
}

static Uint16 getU16(const Uint8 *p)
{
	return p[0] | (Uint16)p[1] << 8;
}

static Uint32 getU32(const Uint8 *p)
{
	return getU16(p) | (Uint32)getU16(p + 2) << 16;
}

// the size of the token at buf[at], or 0 if it is not one (or is cut short)
static size_t tokenSize(size_t at)
{
	if (at >= len)
		return 0;

	size_t size;
	switch (buf[at])
	{
	case TOKEN_CHECK:    size = 5; break;
	case TOKEN_SETTINGS: size = 3; break;
	case TOKEN_SKIP:
	case TOKEN_END:
	case TOKEN_REPEAT:
	case TOKEN_REPEAT | 1:
		size = 1;
		break;
	case TOKEN_INPUT:
	case TOKEN_INPUT | 1:
		size = 1 + INPUT_SIZE;
		break;
	default:
		return 0;
	}
	return at + size <= len ? size : 0;
}

// --- the file ---

static void writeOut(void)
{
	if (file.f == NULL || written >= len)
		return;

	fileWrite(&file, buf + written, len - written);
	fileFlush(&file);
	written = len;

	if (file.error)
	{
		logError("Failed to write to '%s': %s", levelrec_filename, fileGetError(&file));
		fileClose(&file);
	}
}

static bool openForWriting(void)
{
	file = userFileOpen(levelrec_filename, "wb");
	if (file.error)
	{
		logError("Failed to open '%s' for writing: %s", levelrec_filename, fileGetError(&file));
		file = (File){ 0 };
		return false;
	}
	written = 0;
	writeOut();
	return file.f != NULL;
}

static void closeFile(void)
{
	if (file.f != NULL)
	{
		writeOut();
		fileClose(&file);
	}
	file = (File){ 0 };
}

// Reads the file into buf and checks it; keeps only the whole tokens.
static bool loadRecording(void)
{
	if (loaded)
		return true;

	if (mode != MODE_OFF || !userFileExists(levelrec_filename))
		return false;

	File in = userFileOpen(levelrec_filename, "rb");
	if (in.error)
		return false;

	const long length = fileGetLength(&in);
	if (length < (long)sizeof(Header) || length > 64L * 1024 * 1024)
	{
		fileClose(&in);
		return false;
	}

	len = 0;
	reserve((size_t)length);
	fileReadExactly(&in, buf, (size_t)length);
	const bool error = in.error;
	fileClose(&in);
	if (error)
		return false;
	len = (size_t)length;

	memcpy(&header, buf, sizeof(header));
	if (memcmp(header.magic, magic, sizeof(magic)) != 0 ||
	    header.version != FORMAT_VERSION ||
	    header.headerSize != sizeof(Header) ||
	    header.playerSize != sizeof(Player) ||
	    header.saveSize != sizeof(JE_SaveFileType) ||
	    (header.slot != 11 && header.slot != 22))
	{
		logInfo("'%s' is from another version of the game; it is not used.", levelrec_filename);
		len = 0;
		return false;
	}

	loadedEnded = false;
	loadedChecks = 0;
	size_t at = sizeof(Header);
	for (size_t size; (size = tokenSize(at)) > 0; at += size)
	{
		if (buf[at] == TOKEN_CHECK)
			++loadedChecks;
		else if (buf[at] == TOKEN_END)
		{
			loadedEnded = true;
			at += size;
			break;
		}
	}
	len = at;  // a token cut short when the game closed is left out

	loaded = true;
	return true;
}

static void forgetLoaded(void)
{
	loaded = false;
	len = 0;
}

// --- the state ---

static void captureStart(StartState *start)
{
	memset(start, 0, sizeof(*start));

	start->episode = episodeNum;
	start->lvlFileNum = lvlFileNum;
	start->mainLevel = mainLevel;
	start->difficulty = difficultyLevel;
	start->oldDifficulty = oldDifficultyLevel;
	memcpy(start->levelName, levelName, sizeof(start->levelName));
	start->twoPlayerMode = twoPlayerMode;
	start->galagaMode = galagaMode;
	start->onePlayerAction = onePlayerAction;
	start->superTyrian = superTyrian;
	start->extraGame = extraGame;
	start->bonusLevelCurrent = bonusLevelCurrent;
	start->normalBonusLevelCurrent = normalBonusLevelCurrent;
	start->superArcadeMode = superArcadeMode;

	for (unsigned int i = 0; i < COUNTOF(player); ++i)
	{
		start->player[i] = player[i];
		start->player[i].lives = NULL;  // points into the struct itself
	}
	start->cubeMax = cubeMax;
	start->youAreCheating = youAreCheating;
	start->processorType = processorType;
	start->gameSpeed = gameSpeed;
	start->fastPlay = fastPlay;
	start->background2 = background2;
	start->smoothScroll = smoothScroll;
	start->wild = wild;
	start->superWild = superWild;
	start->explosionTransparent = explosionTransparent;
	start->filtrationAvail = filtrationAvail;
	start->displayScore = displayScore;
	start->pentiumMode = pentiumMode;
	start->portConfigDone = portConfigDone;
	start->flareDuration = flareDuration;
	start->specialWeaponWpn = specialWeaponWpn;
	start->flareStart = flareStart;
	start->spraySpecial = spraySpecial;
	start->infiniteShot = infiniteShot;
	start->linkToPlayer = linkToPlayer;
	start->optionAttachmentReturn = optionAttachmentReturn;
	start->flareColChg = flareColChg;
	start->specialWeaponFilter = specialWeaponFilter;
	start->specialWeaponFreq = specialWeaponFreq;
	start->levelFilterNew = levelFilterNew;
	start->doIced = doIced;
	start->explosionFollowAmountX = explosionFollowAmountX;
	start->explosionFollowAmountY = explosionFollowAmountY;
}

static bool sameLevel(const StartState *a, const StartState *b)
{
	return a->episode == b->episode &&
	       a->lvlFileNum == b->lvlFileNum &&
	       a->mainLevel == b->mainLevel &&
	       memcmp(a->levelName, b->levelName, sizeof(a->levelName)) == 0 &&
	       a->twoPlayerMode == b->twoPlayerMode &&
	       a->galagaMode == b->galagaMode &&
	       a->onePlayerAction == b->onePlayerAction &&
	       a->superTyrian == b->superTyrian &&
	       a->extraGame == b->extraGame &&
	       a->bonusLevelCurrent == b->bonusLevelCurrent &&
	       a->normalBonusLevelCurrent == b->normalBonusLevelCurrent &&
	       a->superArcadeMode == b->superArcadeMode;
}

static void restoreStart(const StartState *start)
{
	for (unsigned int i = 0; i < COUNTOF(player); ++i)
	{
		Player recorded = start->player[i];
		recorded.lives = &player[i].items.weapon[i].power;  // as JE_initPlayerData sets it

		Player now = player[i];
		now.lives = recorded.lives;
		if (memcmp(&now, &recorded, sizeof(now)) != 0)
			logDebug("Level recording: player %u restored from the recording.", i + 1);

		player[i] = recorded;
	}
	difficultyLevel = start->difficulty;
	oldDifficultyLevel = start->oldDifficulty;
	cubeMax = start->cubeMax;
	youAreCheating = start->youAreCheating;
	processorType = start->processorType;
	gameSpeed = start->gameSpeed;
	fastPlay = start->fastPlay;
	background2 = start->background2;
	smoothScroll = start->smoothScroll;
	wild = start->wild;
	superWild = start->superWild;
	explosionTransparent = start->explosionTransparent;
	filtrationAvail = start->filtrationAvail;
	displayScore = start->displayScore;
	pentiumMode = start->pentiumMode;
	portConfigDone = start->portConfigDone;
	flareDuration = start->flareDuration;
	specialWeaponWpn = start->specialWeaponWpn;
	flareStart = start->flareStart;
	spraySpecial = start->spraySpecial;
	infiniteShot = start->infiniteShot;
	linkToPlayer = start->linkToPlayer;
	optionAttachmentReturn = start->optionAttachmentReturn;
	flareColChg = start->flareColChg;
	specialWeaponFilter = start->specialWeaponFilter;
	specialWeaponFreq = start->specialWeaponFreq;
	levelFilterNew = start->levelFilterNew;
	doIced = start->doIced;
	explosionFollowAmountX = start->explosionFollowAmountX;
	explosionFollowAmountY = start->explosionFollowAmountY;
}

// the parts of a save that matter for playing (not the name or high score)
static bool sameSave(const JE_SaveFileType *a, const JE_SaveFileType *b)
{
	return a->level == b->level &&
	       memcmp(a->items, b->items, sizeof(a->items)) == 0 &&
	       a->score == b->score &&
	       a->score2 == b->score2 &&
	       memcmp(a->levelName, b->levelName, sizeof(a->levelName)) == 0 &&
	       a->cubes == b->cubes &&
	       memcmp(a->power, b->power, sizeof(a->power)) == 0 &&
	       a->episode == b->episode &&
	       memcmp(a->lastItems, b->lastItems, sizeof(a->lastItems)) == 0 &&
	       a->difficulty == b->difficulty &&
	       a->secretHint == b->secretHint &&
	       a->gameHasRepeated == b->gameHasRepeated &&
	       a->initialDifficulty == b->initialDifficulty;
}

static Uint32 mix(Uint32 hash, Uint32 value)
{
	for (int i = 0; i < 4; ++i)
	{
		hash ^= (value >> (i * 8)) & 0xff;
		hash *= 16777619u;
	}
	return hash;
}

// What a replay that went differently would show soon: the random numbers,
// the ships, the level script, the enemies and the shots.
static Uint32 stateChecksum(void)
{
	Uint32 hash = 2166136261u;
#ifdef LEVELREC_DEBUG
	logInfo("CHK f%u mt%lx p0 %d,%d a%u s%u c%lu al%d wm%u ex%u inv%u p1 %d,%d a%u s%u c%lu al%d wm%u ex%u inv%u loc %u %u bp %u %u %u ek %u te %u",
	        frame, mt_state_checksum(), player[0].x, player[0].y, player[0].armor, player[0].shield, player[0].cash, player[0].is_alive, player[0].weapon_mode, player[0].exploding_ticks, player[0].invulnerable_ticks,
	        player[1].x, player[1].y, player[1].armor, player[1].shield, player[1].cash, player[1].is_alive, player[1].weapon_mode, player[1].exploding_ticks, player[1].invulnerable_ticks,
	        curLoc, eventLoc, backPos, backPos2, backPos3, enemyKilled, totalEnemy);
	for (unsigned int i = 0; i < COUNTOF(enemy); ++i)
		if (enemyAvail[i] != 1)
			logInfo("  e%u av%u %d,%d ar%u", i, enemyAvail[i], enemy[i].ex, enemy[i].ey, enemy[i].armorleft);
#endif

	hash = mix(hash, mt_state_checksum());

	for (unsigned int i = 0; i < COUNTOF(player); ++i)
	{
		const Player *p = &player[i];
		hash = mix(hash, p->x);
		hash = mix(hash, p->y);
		hash = mix(hash, p->armor);
		hash = mix(hash, p->shield);
		hash = mix(hash, p->cash);
		hash = mix(hash, p->is_alive);
		hash = mix(hash, p->weapon_mode);
		hash = mix(hash, p->exploding_ticks);
		hash = mix(hash, p->invulnerable_ticks);
	}

	hash = mix(hash, curLoc);
	hash = mix(hash, eventLoc);
	hash = mix(hash, backPos | (Uint32)backPos2 << 16);
	hash = mix(hash, backPos3);
	hash = mix(hash, enemyKilled | (Uint32)totalEnemy << 16);

	for (unsigned int i = 0; i < COUNTOF(enemy); ++i)
	{
		hash = mix(hash, enemyAvail[i]);
		if (enemyAvail[i] != 1)
		{
			hash = mix(hash, (Uint16)enemy[i].ex | (Uint32)(Uint16)enemy[i].ey << 16);
			hash = mix(hash, enemy[i].armorleft);
		}
	}

	for (unsigned int i = 0; i < MAX_PWEAPON; ++i)
	{
		if (shotAvail[i] != 0)
			hash = mix(hash, i | (Uint32)(Uint16)playerShotData[i].shotX << 8 | (Uint32)(Uint16)playerShotData[i].shotY << 20);
	}

	return hash;
}

// --- recording ---

static bool recordable(void)
{
	// the same cases in which the game saves "LAST LEVEL" when a level starts
	return !isNetworkGame && !playDemo && !recordDemo && !doNotSaveBackup && !timedBattleMode;
}

static void encodeInput(const LevelRecInput *input, Uint8 *out)
{
	Uint32 angle;
	memcpy(&angle, &input->linkGunAngle, sizeof(angle));

	const Uint16 values[4] = { (Uint16)input->dx, (Uint16)input->dy, (Uint16)input->accelX, (Uint16)input->accelY };
	for (int i = 0; i < 4; ++i)
	{
		out[i * 2] = values[i] & 0xff;
		out[i * 2 + 1] = values[i] >> 8;
	}
	out[8] = input->flags;
	for (int i = 0; i < 4; ++i)
		out[9 + i] = (angle >> (i * 8)) & 0xff;
}

static void decodeInput(const Uint8 *in, LevelRecInput *input)
{
	input->dx = (Sint16)getU16(in);
	input->dy = (Sint16)getU16(in + 2);
	input->accelX = (Sint16)getU16(in + 4);
	input->accelY = (Sint16)getU16(in + 6);
	input->flags = in[8];
	const Uint32 angle = getU32(in + 9);
	memcpy(&input->linkGunAngle, &angle, sizeof(angle));
}

static bool sameInput(const LevelRecInput *a, const LevelRecInput *b)
{
	Uint8 ea[INPUT_SIZE], eb[INPUT_SIZE];
	encodeInput(a, ea);
	encodeInput(b, eb);
	return memcmp(ea, eb, sizeof(ea)) == 0;
}

static void stopRecording(void)
{
	putU8(TOKEN_END);
	closeFile();
	mode = MODE_OFF;
}

// --- replaying ---

static void applySettings(Uint8 newProcessorType, Uint8 newGameSpeed)
{
	// as the pause menu does
	processorType = newProcessorType;
	gameSpeed = newGameSpeed;
	JE_initProcessorType();
	JE_setNewGameSpeed();
}

static void endFast(void)
{
	samples_disabled = userSamplesDisabled;
	if (processorType != userProcessorType || gameSpeed != userGameSpeed)
		applySettings(userProcessorType, userGameSpeed);
}

// The fast replay reached where the game closed: from here the level is played.
static void handOver(void)
{
	logInfo("Level resumed after %u frames.", frame);

	endFast();

	len = pos;  // anything after is not a whole token
	mode = MODE_RECORD;
	if (!openForWriting())
		mode = MODE_OFF;

	// let the player get ready, once a frame of the level (and not of the
	// fast replay) is on the screen
	pausePending = true;
	ingamemenu_pressed = false;
}

static void fail(const char *what)
{
	logWarn("Level recording: %s (frame %u); the replay stops.", what, frame);
	failed = true;

	if (mode == MODE_FAST)
	{
		endFast();

		// do not try this recording again
		len = pos;
		putU8(TOKEN_END);
		if (openForWriting())
			closeFile();

		mode = MODE_OFF;

		// back to the start of the level, as after losing the ship
		JE_drawTextWindow(tr("Could not resume the level."));
		JE_showVGA();
		SDL_Delay(2500);

		reallyEndLevel = true;
		playerEndLevel = true;
	}
	else
	{
		mode = MODE_OFF;
		reallyEndLevel = true;
	}
}

static void endOfRecording(void)
{
	if (mode == MODE_FAST)
	{
		handOver();
	}
	else
	{
		logDebug("Replay finished after %u frames.", frame);
		mode = MODE_OFF;
		reallyEndLevel = true;
	}
}

// --- the title screen ---

bool levelrec_can_resume(int slot)
{
	if (slot != 11 && slot != 22)
		return false;
	if (!loadRecording())
		return false;

	return !loadedEnded && loadedChecks > 1 && header.slot == slot &&
	       sameSave(&header.save, &saveFiles[slot - 1]);
}

void levelrec_request_resume(bool request)
{
	resumeRequested = request;
}

bool levelrec_can_watch(void)
{
	return loadRecording() && loadedChecks > 1;
}

bool levelrec_start_watch(void)
{
	if (!levelrec_can_watch())
		return false;

	// load the save of the recording without changing the saved games
	const JE_byte slot = header.slot;
	const JE_SaveFileType keep = saveFiles[slot - 1];
	const int keepLastSaveSlot = lastSaveSlot;

	saveFiles[slot - 1] = header.save;
	JE_loadGame(slot);
	saveFiles[slot - 1] = keep;
	lastSaveSlot = keepLastSaveSlot;

	gameLoaded = true;
	gameJustLoaded = true;
	watchRequested = true;
	return true;
}

Uint8 levelrec_resume_section(void)
{
	return (resumeRequested || watchRequested) && loaded ? header.start.mainLevel : 0;
}

// --- the level ---

void levelrec_level_setup(void)
{
	const bool resume = resumeRequested, watch = watchRequested;
	resumeRequested = watchRequested = false;
	watchLevel = false;
	failed = false;
	pausePending = false;
	headerPending = false;
	frame = 0;
	haveLastInput[0] = haveLastInput[1] = false;

	closeFile();
	mode = MODE_OFF;

	if (resume || watch)
	{
		StartState now;
		captureStart(&now);

		if (loadRecording() && sameLevel(&now, &header.start))
		{
			userProcessorType = processorType;
			userGameSpeed = gameSpeed;

			restoreStart(&header.start);
			lastProcessorType = processorType;
			lastGameSpeed = gameSpeed;

			mt_srand(header.seed);
			pos = sizeof(Header);
			loaded = false;  // buf is in use now

			if (watch)
			{
				mode = MODE_WATCH;
				watchLevel = true;
				logInfo("Replaying the last level (%s).", levelName);
			}
			else
			{
				mode = MODE_FAST;
				userSamplesDisabled = samples_disabled;
				samples_disabled = true;
				lastShownTicks = 0;
				logInfo("Resuming the level %s.", levelName);
			}
			return;
		}

		logWarn("Level recording: it is not of this level; %s.", watch ? "no replay" : "the level starts from the beginning");
#ifdef LEVELREC_DEBUG
		logInfo("now ep%u lvl%u main%u diff%d %s 2p%d gal%d 1pa%d st%d ex%d bon%d nb%d sa%u", now.episode, now.lvlFileNum, now.mainLevel, now.difficulty, now.levelName, now.twoPlayerMode, now.galagaMode, now.onePlayerAction, now.superTyrian, now.extraGame, now.bonusLevelCurrent, now.normalBonusLevelCurrent, now.superArcadeMode);
		const StartState *r = &header.start;
		logInfo("rec ep%u lvl%u main%u diff%d %s 2p%d gal%d 1pa%d st%d ex%d bon%d nb%d sa%u", r->episode, r->lvlFileNum, r->mainLevel, r->difficulty, r->levelName, r->twoPlayerMode, r->galagaMode, r->onePlayerAction, r->superTyrian, r->extraGame, r->bonusLevelCurrent, r->normalBonusLevelCurrent, r->superArcadeMode);
#endif
		forgetLoaded();

		if (watch)
		{
			watchLevel = true;
			reallyEndLevel = true;
			return;
		}
	}

	forgetLoaded();

	if (!recordable())
		return;

	memset(&header, 0, sizeof(header));
	captureStart(&header.start);

	header.seed = mt_rand();  // the game's random numbers go on as before
	mt_srand(header.seed);

	lastProcessorType = processorType;
	lastGameSpeed = gameSpeed;
	mode = MODE_RECORD;
	headerPending = true;
}

void levelrec_level_begin(void)
{
	const JE_byte slot = twoPlayerMode ? 22 : 11;

	if (mode == MODE_FAST && !sameSave(&header.save, &saveFiles[slot - 1]))
		logDebug("Level recording: the save made now differs from the recorded one.");

	if (mode != MODE_RECORD || !headerPending)
		return;
	headerPending = false;

	memcpy(header.magic, magic, sizeof(magic));
	header.version = FORMAT_VERSION;
	header.headerSize = sizeof(Header);
	header.playerSize = sizeof(Player);
	header.saveSize = sizeof(JE_SaveFileType);
	header.slot = slot;
	header.save = saveFiles[slot - 1];

	len = 0;
	reserve(sizeof(header));
	memcpy(buf, &header, sizeof(header));
	len = sizeof(header);

	if (!openForWriting())
		mode = MODE_OFF;
}

void levelrec_frame(void)
{
	if (pausePending)
	{
		pausePending = false;
		pause_pressed = true;
	}

	if (mode == MODE_FAST || mode == MODE_WATCH)
	{
		for (; ; )
		{
			if (pos >= len || buf[pos] == TOKEN_END)
			{
				endOfRecording();
				break;
			}
			if (buf[pos] != TOKEN_SETTINGS)
				break;

			applySettings(buf[pos + 1], buf[pos + 2]);
			lastProcessorType = processorType;
			lastGameSpeed = gameSpeed;
			pos += 3;
		}
	}

	switch (mode)
	{
	case MODE_OFF:
		return;

	case MODE_RECORD:  // also right after the fast replay handed over
		if (processorType != lastProcessorType || gameSpeed != lastGameSpeed)
		{
			lastProcessorType = processorType;
			lastGameSpeed = gameSpeed;
			putU8(TOKEN_SETTINGS);
			putU8(processorType);
			putU8(gameSpeed);
		}
		if (frame % CHECK_INTERVAL == 0)
		{
			putU8(TOKEN_CHECK);
			putU32(stateChecksum());
		}
		++frame;
		writeOut();
		return;

	case MODE_FAST:
	case MODE_WATCH:
		if (frame % CHECK_INTERVAL == 0)
		{
			if (buf[pos] != TOKEN_CHECK)
			{
				fail("the recording is out of step");
				return;
			}
			const Uint32 recorded = getU32(&buf[pos + 1]);
			pos += 5;
			if (recorded != stateChecksum())
			{
				fail("the replay went differently");
				return;
			}
		}
		++frame;
		return;
	}
}

bool levelrec_player_input(unsigned int p, LevelRecInput *input)
{
	if (p > 1)
		return false;

	if ((mode == MODE_FAST || mode == MODE_WATCH) && (pos >= len || buf[pos] == TOKEN_END))
		endOfRecording();  // from here the input is the player's (or a Replay ends)

	switch (mode)
	{
	case MODE_OFF:
		return false;

	case MODE_RECORD:  // also right after the fast replay handed over
		if (haveLastInput[p] && sameInput(input, &lastInput[p]))
		{
			putU8(TOKEN_REPEAT | p);
		}
		else
		{
			Uint8 data[INPUT_SIZE];
			encodeInput(input, data);
			putU8(TOKEN_INPUT | p);
			for (size_t i = 0; i < INPUT_SIZE; ++i)
				putU8(data[i]);
			lastInput[p] = *input;
			haveLastInput[p] = true;
		}
		return false;

	case MODE_FAST:
	case MODE_WATCH:
		if (buf[pos] == (TOKEN_INPUT | p))
		{
			decodeInput(&buf[pos + 1], &lastInput[p]);
			haveLastInput[p] = true;
			pos += 1 + INPUT_SIZE;
		}
		else if (buf[pos] == (TOKEN_REPEAT | p) && haveLastInput[p])
		{
			pos += 1;
		}
		else
		{
			fail("the recording is out of step");
			return false;
		}
		*input = lastInput[p];
		return true;
	}
	return false;
}

void levelrec_note_skip(void)
{
	if (mode == MODE_RECORD)
		putU8(TOKEN_SKIP);
}

bool levelrec_take_skip(void)
{
	if ((mode == MODE_FAST || mode == MODE_WATCH) && pos < len && buf[pos] == TOKEN_SKIP)
	{
		++pos;
		return true;
	}
	return false;
}

// What the cheat and debug keys can change.
static Uint32 cheatState(void)
{
	Uint32 hash = 2166136261u;
	for (unsigned int i = 0; i < COUNTOF(player); ++i)
	{
		const Uint8 *items = (const Uint8 *)&player[i].items;
		for (size_t j = 0; j < sizeof(player[i].items); ++j)
			hash = mix(hash, items[j]);
		hash = mix(hash, player[i].armor);
		hash = mix(hash, player[i].weapon_mode);
	}
	for (unsigned int i = 0; i < COUNTOF(smoothies); ++i)
		hash = mix(hash, smoothies[i]);
	hash = mix(hash, youAreCheating | superPause << 1 | debug << 2 | levelTimer << 3 | endLevel << 4);
	hash = mix(hash, levelTimerCountdown | (Uint32)levelEnd << 16);
	hash = mix(hash, (Uint8)levelFilter | (Uint32)fastPlay << 8 | (Uint32)shipGr << 16);
	return hash;
}

void levelrec_cheats_begin(void)
{
	if (mode == MODE_RECORD)
		cheatHash = cheatState();
}

void levelrec_cheats_end(void)
{
	if (mode == MODE_RECORD && cheatState() != cheatHash)
	{
		logInfo("Level recording: a cheat or debug key changed the level; it stops here.");
		stopRecording();
	}
}

void levelrec_level_end(void)
{
	switch (mode)
	{
	case MODE_OFF:
		break;
	case MODE_RECORD:
		stopRecording();
		break;
	case MODE_FAST:
		endFast();
		mode = MODE_OFF;
		break;
	case MODE_WATCH:
		mode = MODE_OFF;
		break;
	}
	headerPending = false;
}

bool levelrec_end_watch(void)
{
	const bool watched = watchLevel;
	watchLevel = false;
	return watched;
}

void levelrec_close(void)
{
	// no TOKEN_END: Continue resumes the level
	if (mode == MODE_RECORD)
		closeFile();
}

bool levelrec_replaying(void)
{
	return mode == MODE_FAST || mode == MODE_WATCH;
}

bool levelrec_fast(void)
{
	return mode == MODE_FAST;
}

bool levelrec_watching(void)
{
	return watchLevel;
}

bool levelrec_failed(void)
{
	return failed;
}

bool levelrec_fast_frame_due(void)
{
	const Uint32 now = SDL_GetTicks();
	if (lastShownTicks != 0 && now - lastShownTicks < 50)
		return false;
	lastShownTicks = now;
	return true;
}

void levelrec_draw_progress(SDL_Surface *surface)
{
	const size_t total = len > sizeof(Header) ? len - sizeof(Header) : 1;
	const unsigned int percent = (unsigned int)((pos - sizeof(Header)) * 100 / total);

	char text[64];
	snprintf(text, sizeof(text), tr("Resuming the level... %u%%"), percent);
	JE_dString(surface, 130 - JE_textWidth(text, SMALL_FONT_SHAPES) / 2, 90, text, SMALL_FONT_SHAPES);
}
