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
/* Headless test for the statistics and level records (src/stats.c): plays
 * pretend levels by setting the game's counters, then reads statistics.cfg
 * back.
 *
 * Usage: XDG_CONFIG_HOME=<empty dir> test_stats */
#include "config.h"
#include "episodes.h"
#include "player.h"
#include "stats.h"
#include "varz.h"

#include <stdio.h>
#include <string.h>

static int fails = 0;

static void check(bool ok, const char *what)
{
	printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
	fails += !ok;
}

// one level: the cash earned, the frames it took, enemies killed of 10
static void playLevel(const char *name, unsigned long earned, unsigned int frames, int killed, bool dies)
{
	episodeNum = 1;
	snprintf(levelName, sizeof(levelName), "%-10s", name);  // padded like the data
	playerEndLevel = false;
	bonusLevel = false;
	player[0].is_alive = true;

	stats_level_start();
	for (unsigned int i = 0; i < frames; ++i)
		stats_level_frame();

	player[0].cash += earned;
	enemyKilled = killed;
	totalEnemy = 10;
	cubeMax = 1;
	player[0].is_alive = !dies;

	stats_level_end();
}

int main(void)
{
	twoPlayerMode = false;
	galagaMode = false;
	player[0].cash = 1000;

	stats_load();
	check(stats_totals.levelsCompleted == 0, "starts empty");

	playLevel("TYRIAN", 5000, 35 * 90, 8, false);
	check(!stats_new_record, "the first time is not a new record");
	check(stats_totals.levelsCompleted == 1 && stats_totals.enemiesDestroyed == 8, "level and enemies counted");
	check(stats_totals.datacubes == 1 && stats_totals.highestScore == 6000, "datacubes and highest score");

	playLevel("TYRIAN", 1000, 35 * 120, 5, true);
	check(!stats_new_record && stats_totals.deaths == 1, "a death counts and sets no record");
	check(stats_totals.levelsCompleted == 1 && stats_totals.datacubes == 1, "a lost level is not completed");

	playLevel("TYRIAN", 2000, 35 * 60, 6, false);
	check(stats_new_record, "faster: new record");

	playLevel("BUBBLES", 3000, 35 * 100, 7, false);
	check(!stats_new_record, "another level gets its own records");

	// read the file back
	stats_load();
	check(stats_totals.levelsCompleted == 3 && stats_totals.deaths == 1, "totals read back");
	check(stats_totals.enemiesDestroyed == 26, "enemies read back");
	check(stats_totals.levelTicks == 35 * (90 + 120 + 60 + 100), "time read back");

	playLevel("TYRIAN", 5001, 35 * 200, 0, false);
	check(stats_new_record, "more cash: new record (records read back)");

	playLevel("TYRIAN", 10, 35 * 200, 0, false);
	check(!stats_new_record, "worse in everything: no record");

	playLevel("BUBBLES", 10, 35 * 200, 8, false);
	check(stats_new_record, "the second level's records were read back too");

	printf(fails ? "test_stats: %d FAILED\n" : "test_stats: all ok\n", fails);
	return fails != 0;
}
