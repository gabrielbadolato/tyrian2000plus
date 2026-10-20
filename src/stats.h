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
#ifndef STATS_H
#define STATS_H

#include <stdbool.h>

// totals of every game played, kept in statistics.cfg in the user folder
typedef struct
{
	unsigned int levelTicks;  // level frames; STATS_TICKS_PER_SECOND each second
	unsigned int levelsCompleted;
	unsigned int enemiesDestroyed;
	unsigned int deaths;
	unsigned int secretLevels;
	unsigned int datacubes;
	unsigned long highestScore;
} StatsTotals;

// the best of each level, by episode and level name
typedef struct
{
	int episode;
	char name[11];
	unsigned long bestScore;  // cash earned in the level
	unsigned int bestTicks;   // fewest frames
	int bestPercent;          // enemies destroyed
} StatsLevelRecord;

// the level loop runs at about 35 frames a second at the normal game speed;
// times are counted in frames, so pauses and the game speed do not change them
#define STATS_TICKS_PER_SECOND 35

extern StatsTotals stats_totals;

void stats_load(void);

// called by JE_main: when a level starts, once per frame of the level loop,
// and when the level ends (before the end of level screen)
void stats_level_start(void);
void stats_level_frame(void);
void stats_level_end(void);

// set by stats_level_end() when the level beat one of its records
extern bool stats_new_record;

// full screen page with the totals and the records of each level; until
// Esc, B or a right click
void statisticsScreen(void);

#endif /* STATS_H */
