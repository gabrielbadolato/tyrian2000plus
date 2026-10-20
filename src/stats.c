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
 * Statistics: totals of every game played and the records of each level.
 * They are kept in statistics.cfg in the user folder, apart from the game's
 * own tyrian.sav and tyrian.cfg, which stay as they are.  The level loop
 * counts the frames of the level; the end of the level reads the counters
 * the game already has (enemies destroyed, datacubes, cash).
 */
#include "stats.h"

#include "config.h"
#include "config_file.h"
#include "demo.h"
#include "episodes.h"
#include "file.h"
#include "font.h"
#include "fonthand.h"
#include "joystick.h"
#include "keyboard.h"
#include "lang.h"
#include "levelrec.h"
#include "logging.h"
#include "menus.h"
#include "mouse.h"
#include "nortsong.h"
#include "opentyr.h"
#include "palette.h"
#include "params.h"
#include "picload.h"
#include "player.h"
#include "sndmast.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

StatsTotals stats_totals;
bool stats_new_record;

#define MAX_RECORDS 256

static StatsLevelRecord records[MAX_RECORDS];
static int recordCount;

static const char *const statsFilename = "statistics.cfg";

// the level being played
static bool levelCounting;
static unsigned int levelTicks;
static unsigned long levelStartCash;
static int levelEpisode;
static char levelRecordName[11];

static unsigned long totalCash(void)
{
	// in Galaga mode player 2 is a sidekick and player 1 gets the cash
	return player[0].cash + (twoPlayerMode && !galagaMode ? player[1].cash : 0);
}

// level names are padded with spaces in the data
static void trimmedName(char *dst, size_t size, const char *src)
{
	snprintf(dst, size, "%s", src);
	size_t len = strlen(dst);
	while (len > 0 && dst[len - 1] == ' ')
		dst[--len] = '\0';
}

static StatsLevelRecord *findRecord(int episode, const char *name, bool add)
{
	for (int i = 0; i < recordCount; ++i)
		if (records[i].episode == episode && strcmp(records[i].name, name) == 0)
			return &records[i];

	if (!add || recordCount >= MAX_RECORDS)
		return NULL;

	StatsLevelRecord *record = &records[recordCount++];
	memset(record, 0, sizeof(*record));
	record->episode = episode;
	snprintf(record->name, sizeof(record->name), "%s", name);
	return record;
}

static unsigned int getUint(const ConfigSection *section, const char *key)
{
	const char *value;
	if (!config_get_string_option(section, key, &value))
		return 0;
	return (unsigned int)strtoul(value, NULL, 10);
}

static unsigned long getUlong(const ConfigSection *section, const char *key)
{
	const char *value;
	if (!config_get_string_option(section, key, &value))
		return 0;
	return strtoul(value, NULL, 10);
}

static void setUlong(ConfigSection *section, const char *key, unsigned long value)
{
	char buffer[24];
	snprintf(buffer, sizeof(buffer), "%lu", value);
	config_set_string_option(section, key, buffer);
}

void stats_load(void)
{
	memset(&stats_totals, 0, sizeof(stats_totals));
	recordCount = 0;

	File file = userFileOpen(statsFilename, "r");
	if (file.error)
		return;  // nothing played yet

	Config config;
	config_init(&config);

	const bool success = config_parse(&config, file.f);
	fileClose(&file);

	if (!success)
	{
		logError("Failed to parse '%s'.", statsFilename);
		config_deinit(&config);
		return;
	}

	const ConfigSection *section = config_find_section(&config, "totals", NULL);
	if (section != NULL)
	{
		stats_totals.levelTicks = getUint(section, "level_frames");
		stats_totals.levelsCompleted = getUint(section, "levels_completed");
		stats_totals.enemiesDestroyed = getUint(section, "enemies_destroyed");
		stats_totals.deaths = getUint(section, "deaths");
		stats_totals.secretLevels = getUint(section, "secret_levels");
		stats_totals.datacubes = getUint(section, "datacubes");
		stats_totals.highestScore = getUlong(section, "highest_score");
	}

	for (unsigned int i = 0; i < config.sections_count && recordCount < MAX_RECORDS; ++i)
	{
		section = &config.sections[i];
		if (strcmp(config_string_to_cstr(&section->type), "level") != 0)
			continue;

		const char *name;
		if (!config_get_string_option(section, "name", &name))
			continue;

		const unsigned int episode = getUint(section, "episode");
		if (episode > 99)
			continue;  // not a real episode

		StatsLevelRecord *record = findRecord((int)episode, name, true);
		if (record == NULL)
			break;
		record->bestScore = getUlong(section, "best_score");
		record->bestTicks = getUint(section, "best_frames");
		record->bestPercent = (int)getUint(section, "best_destroyed");
	}

	config_deinit(&config);
}

static void saveStats(void)
{
	Config config;
	config_init(&config);

	ConfigSection *section = config_add_section_len(&config, "totals", strlen("totals"), NULL, 0);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	config_set_uint_option(section, "level_frames", stats_totals.levelTicks);
	config_set_uint_option(section, "levels_completed", stats_totals.levelsCompleted);
	config_set_uint_option(section, "enemies_destroyed", stats_totals.enemiesDestroyed);
	config_set_uint_option(section, "deaths", stats_totals.deaths);
	config_set_uint_option(section, "secret_levels", stats_totals.secretLevels);
	config_set_uint_option(section, "datacubes", stats_totals.datacubes);
	setUlong(section, "highest_score", stats_totals.highestScore);

	for (int i = 0; i < recordCount; ++i)
	{
		section = config_add_section_len(&config, "level", strlen("level"), NULL, 0);
		if (section == NULL)
			exit(EXIT_FAILURE);  // out of memory

		config_set_uint_option(section, "episode", (unsigned int)records[i].episode);
		config_set_string_option(section, "name", records[i].name);
		setUlong(section, "best_score", records[i].bestScore);
		config_set_uint_option(section, "best_frames", records[i].bestTicks);
		config_set_uint_option(section, "best_destroyed", (unsigned int)records[i].bestPercent);
	}

	File file = userFileOpen(statsFilename, "w");
	if (file.error)
	{
		logError("Failed to open '%s': %s", statsFilename, fileGetError(&file));
		config_deinit(&config);
		return;
	}

	config_write(&config, file.f);
	file.error |= ferror(file.f) != 0;

	fileFlush(&file);

	if (file.error)
		logError("Failed to write to '%s': %s", statsFilename, fileGetError(&file));

	fileClose(&file);
	config_deinit(&config);
}

void stats_level_start(void)
{
	// demos and cheats do not count
	levelCounting = !playDemo && !levelrec_watching() && !constantPlay && !constantDie;
	levelTicks = 0;
	levelStartCash = totalCash();
	levelEpisode = episodeNum;
	trimmedName(levelRecordName, sizeof(levelRecordName), levelName);
}

void stats_level_frame(void)
{
	if (levelCounting)
		++levelTicks;
}

void stats_level_end(void)
{
	stats_new_record = false;

	if (!levelCounting || playDemo)
		return;
	levelCounting = false;

	if (youAreCheating)
		return;

	const bool completed = !all_players_dead() && !playerEndLevel;

	stats_totals.levelTicks += levelTicks;
	stats_totals.enemiesDestroyed += enemyKilled;

	// a quit is not a death; in Galaga mode player 2 is only a sidekick
	if (!playerEndLevel)
	{
		stats_totals.deaths += !player[0].is_alive;
		if (twoPlayerMode && !galagaMode)
			stats_totals.deaths += !player[1].is_alive;
	}

	for (unsigned int i = 0; i < (twoPlayerMode ? 2u : 1u); ++i)
		if (player[i].cash > stats_totals.highestScore)
			stats_totals.highestScore = player[i].cash;

	if (completed)
	{
		++stats_totals.levelsCompleted;
		stats_totals.datacubes += cubeMax;
		if (bonusLevel)
			++stats_totals.secretLevels;

		const unsigned long cash = totalCash();
		const unsigned long score = cash > levelStartCash ? cash - levelStartCash : 0;
		const int percent = totalEnemy == 0 ? 0 : (int)(enemyKilled * 100 / totalEnemy);

		StatsLevelRecord *record = levelRecordName[0] != '\0' ? findRecord(levelEpisode, levelRecordName, false) : NULL;
		if (record != NULL)
		{
			// beating a record: only when there was one to beat
			if (score > record->bestScore)
			{
				record->bestScore = score;
				stats_new_record = true;
			}
			if (levelTicks < record->bestTicks)
			{
				record->bestTicks = levelTicks;
				stats_new_record = true;
			}
			if (percent > record->bestPercent)
			{
				record->bestPercent = percent;
				stats_new_record = true;
			}
		}
		else if (levelRecordName[0] != '\0')
		{
			record = findRecord(levelEpisode, levelRecordName, true);
			if (record != NULL)
			{
				record->bestScore = score;
				record->bestTicks = levelTicks;
				record->bestPercent = percent;
			}
		}
	}

	saveStats();
}

/* -------- the Statistics screen -------- */

typedef enum
{
	LINE_BLANK,
	LINE_HEADING,
	LINE_VALUE,    // label and value
	LINE_COLUMNS,  // titles of the record columns
	LINE_RECORD,
} StatsLineStyle;

typedef struct
{
	StatsLineStyle style;
	char text[40];
	char value[24];
	char time[12];
	char percent[8];
} StatsLine;

#define MAX_LINES (MAX_RECORDS + 64)

static StatsLine lines[MAX_LINES];
static int lineCount;

enum
{
	TEXT_X = 14,
	LIST_Y = 26,
	LINE_HEIGHT = 11,
	VISIBLE_LINES = 14,
	SCORE_RIGHT = 196,
	TIME_RIGHT = 248,
	PERCENT_RIGHT = 300,
};

static StatsLine *addLine(StatsLineStyle style, const char *text)
{
	if (lineCount >= MAX_LINES)
		return &lines[MAX_LINES - 1];

	StatsLine *line = &lines[lineCount++];
	memset(line, 0, sizeof(*line));
	line->style = style;
	snprintf(line->text, sizeof(line->text), "%s", text);
	return line;
}

static void formatTicks(char *buffer, size_t size, unsigned int ticks)
{
	const unsigned int seconds = ticks / STATS_TICKS_PER_SECOND;
	if (seconds >= 3600)
		snprintf(buffer, size, "%u:%02u:%02u", seconds / 3600, seconds / 60 % 60, seconds % 60);
	else
		snprintf(buffer, size, "%u:%02u", seconds / 60, seconds % 60);
}

static void addValue(const char *label, unsigned long value)
{
	StatsLine *line = addLine(LINE_VALUE, tr(label));
	snprintf(line->value, sizeof(line->value), "%lu", value);
}

static void buildLines(void)
{
	lineCount = 0;

	addLine(LINE_HEADING, tr("All Games"));

	StatsLine *line = addLine(LINE_VALUE, tr("Time in Levels"));
	formatTicks(line->value, sizeof(line->value), stats_totals.levelTicks);

	addValue("Levels Completed", stats_totals.levelsCompleted);
	addValue("Enemies Destroyed", stats_totals.enemiesDestroyed);
	addValue("Deaths", stats_totals.deaths);
	addValue("Secret Levels Found", stats_totals.secretLevels);
	addValue("Datacubes Found", stats_totals.datacubes);
	addValue("Highest Score", stats_totals.highestScore);

	addLine(LINE_BLANK, "");
	addLine(LINE_HEADING, tr("Level Records"));

	if (recordCount == 0)
	{
		addLine(LINE_VALUE, tr("Finish a level to set its records."));
		return;
	}

	// by episode, in the order the levels were first finished
	int firstEpisode = records[0].episode, lastEpisodeFound = records[0].episode;
	for (int i = 1; i < recordCount; ++i)
	{
		firstEpisode = MIN(firstEpisode, records[i].episode);
		lastEpisodeFound = MAX(lastEpisodeFound, records[i].episode);
	}

	static StatsLevelRecord sorted[MAX_RECORDS];
	int sortedCount = 0;
	for (int episode = firstEpisode; episode <= lastEpisodeFound; ++episode)
		for (int i = 0; i < recordCount; ++i)
			if (records[i].episode == episode)
				sorted[sortedCount++] = records[i];

	int lastEpisode = -1;
	for (int i = 0; i < sortedCount; ++i)
	{
		const StatsLevelRecord *record = &sorted[i];

		if (record->episode != lastEpisode)
		{
			lastEpisode = record->episode;
			addLine(LINE_BLANK, "");

			char title[40];
			if (record->episode >= 1 && record->episode < (int)COUNTOF(episode_name) && episode_name[record->episode][0] != '\0')
				snprintf(title, sizeof(title), "%s", episode_name[record->episode]);
			else
				snprintf(title, sizeof(title), tr("Episode %d"), record->episode);
			line = addLine(LINE_COLUMNS, title);
		}

		line = addLine(LINE_RECORD, record->name);
		snprintf(line->value, sizeof(line->value), "%lu", record->bestScore);
		formatTicks(line->time, sizeof(line->time), record->bestTicks);
		snprintf(line->percent, sizeof(line->percent), "%d%%", record->bestPercent);
	}
}

static void drawLines(int top)
{
	// darken the background behind the text so it reads well
	JE_barShade(VGAScreen, 4, LIST_Y - 3, 315, LIST_Y + VISIBLE_LINES * LINE_HEIGHT);
	JE_barShade(VGAScreen, 4, LIST_Y - 3, 315, LIST_Y + VISIBLE_LINES * LINE_HEIGHT);

	for (int i = 0; i < VISIBLE_LINES && top + i < lineCount; ++i)
	{
		const StatsLine *line = &lines[top + i];
		const int y = LIST_Y + i * LINE_HEIGHT;

		switch (line->style)
		{
		case LINE_HEADING:
			drawFontHvShadow(VGAScreen, TEXT_X, y - 1, line->text, FONT_NORMAL, 15, -1, false, 2);
			break;
		case LINE_VALUE:
			drawFontHvShadow(VGAScreen, TEXT_X, y, line->text, FONT_SMALL, 15, 2, false, 1);
			drawFontHvShadowAligned(VGAScreen, PERCENT_RIGHT, y, line->value, FONT_SMALL, ALIGN_RIGHT, 15, 4, false, 1);
			break;
		case LINE_COLUMNS:
			drawFontHvShadow(VGAScreen, TEXT_X, y, line->text, FONT_SMALL, 15, 6, false, 1);
			drawFontHvShadowAligned(VGAScreen, SCORE_RIGHT, y, tr("Score"), FONT_SMALL, ALIGN_RIGHT, 14, 2, false, 1);
			drawFontHvShadowAligned(VGAScreen, TIME_RIGHT, y, tr("Time"), FONT_SMALL, ALIGN_RIGHT, 14, 2, false, 1);
			drawFontHvShadowAligned(VGAScreen, PERCENT_RIGHT, y, tr("Destroyed"), FONT_SMALL, ALIGN_RIGHT, 14, 2, false, 1);
			break;
		case LINE_RECORD:
			drawFontHvShadow(VGAScreen, TEXT_X + 8, y, line->text, FONT_SMALL, 15, 2, false, 1);
			drawFontHvShadowAligned(VGAScreen, SCORE_RIGHT, y, line->value, FONT_SMALL, ALIGN_RIGHT, 15, 4, false, 1);
			drawFontHvShadowAligned(VGAScreen, TIME_RIGHT, y, line->time, FONT_SMALL, ALIGN_RIGHT, 15, 4, false, 1);
			drawFontHvShadowAligned(VGAScreen, PERCENT_RIGHT, y, line->percent, FONT_SMALL, ALIGN_RIGHT, 15, 4, false, 1);
			break;
		default:
			break;
		}
	}

	// scroll bar
	if (lineCount > VISIBLE_LINES)
	{
		const int h = VISIBLE_LINES * LINE_HEIGHT;
		const int barH = MAX(6, h * VISIBLE_LINES / lineCount);
		const int barY = LIST_Y + (h - barH) * top / (lineCount - VISIBLE_LINES);
		fill_rectangle_xy(VGAScreen, 312, LIST_Y, 313, LIST_Y + h - 1, 0);
		fill_rectangle_xy(VGAScreen, 312, barY, 313, barY + barH - 1, 15 * 16 + 8);
	}
}

void statisticsScreen(void)
{
	JE_loadPic(VGAScreen2, 2, false);
	fill_rectangle_wh(VGAScreen2, 0, 192, 320, 8, 0);

	buildLines();

	const int maxTop = MAX(0, lineCount - VISIBLE_LINES);
	bool fadeIn = true;
	int top = 0;

	for (; ; )
	{
		setFrameCount(2);

		memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);

		drawFontHvShadowAligned(VGAScreen, 160, 4, tr("Statistics"), FONT_LARGE, ALIGN_CENTER, 15, -3, false, 2);

		drawLines(top);

		const int j = joystick_first_gamepad();
		char help[128];
		snprintf(help, sizeof(help), tr("Up/Down: scroll. Left/Right: page. Esc (%s): back."), joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
		JE_textShade(VGAScreen, TEXT_X, 190, help, 15, 4, PART_SHADE);

		if (fadeIn)
		{
			fade_palette(colors, 10, 0, 255);
			fadeIn = false;
		}

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		waitUntilElapsed();

		const int oldTop = top;
		bool done = false;

		KeyboardInput keyboardInput;
		while (keyboardGetInput(&keyboardInput))
		{
			switch (keyboardInput.scancode)
			{
			case SDL_SCANCODE_UP:
				top -= 1;
				break;
			case SDL_SCANCODE_DOWN:
				top += 1;
				break;
			case SDL_SCANCODE_LEFT:
			case SDL_SCANCODE_PAGEUP:
				top -= VISIBLE_LINES - 1;
				break;
			case SDL_SCANCODE_RIGHT:
			case SDL_SCANCODE_PAGEDOWN:
				top += VISIBLE_LINES - 1;
				break;
			case SDL_SCANCODE_HOME:
				top = 0;
				break;
			case SDL_SCANCODE_END:
				top = maxTop;
				break;
			case SDL_SCANCODE_ESCAPE:
			case SDL_SCANCODE_RETURN:
				done = true;
				break;
			default:
				break;
			}
		}

		MouseInput mouseInput;
		while (mouseGetInput(INPUT_NO_MOTION, &mouseInput))
		{
			if (mouseInput.button == SDL_BUTTON_RIGHT || mouseInput.button == SDL_BUTTON_LEFT)
				done = true;
		}

		top = MAX(0, MIN(top, maxTop));
		if (top != oldTop)
			JE_playSampleNum(S_CURSOR);

		if (done)
		{
			JE_playSampleNum(S_SPRING);
			return;
		}
	}
}
