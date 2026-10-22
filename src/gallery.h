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
#ifndef GALLERY_H
#define GALLERY_H

#include "opentyr.h"

#include <stdbool.h>
#include <stdio.h>

// The Gallery (Setup > "Gallery..."): the ships, weapons, sidekicks and other
// items the player has come across, and the enemies met in the levels, drawn
// with the game's own pictures, and a page with the statistics.  What has been seen is kept in gallery.cfg in
// the user folder, apart from tyrian.sav; recording it only reads the game.

// Reads gallery.cfg (at start).
void gallery_load(void);

// The level loop (src/tyrian2.c): the start and end of a level and each of
// its frames, an enemy of the level made in slot i (0..99), one made by
// another enemy or at random (JE_newEnemy: never a piece of something
// bigger) and an enemy destroyed.
void gallery_level_start(void);
void gallery_level_frame(void);
void gallery_level_end(void);
void gallery_enemy_made(unsigned int slot);
void gallery_enemy_launched(unsigned int slot);
void gallery_enemy_killed(unsigned int slot);

// Writes gallery.cfg if something was seen since (on quitting the game).
void gallery_save(void);

// The items of the ship's upgrade menu (itemAvail) were loaded.
void gallery_shop_items(void);

// Tests: record also with constant play (-c), which levels normally do not.
extern bool gallery_test_record;

// Tests: without the list of the game's enemies (src/gallery_catalog.h).
extern bool gallery_test_no_list;

// Tests: each enemy recorded is also written here, with the enemyDat index
// of its pieces (to make the list of the game's enemies).
extern FILE *gallery_dump;

// How many entries the gallery has seen, of each kind (tests).  The last
// page is the statistics (src/stats.c), which has no entries.
typedef enum
{
	GALLERY_SHIPS,
	GALLERY_WEAPONS,
	GALLERY_SIDEKICKS,
	GALLERY_OTHER,
	GALLERY_ENEMIES,
	GALLERY_STATS,
	GALLERY_TABS
} GalleryTab;
unsigned int gallery_seen_count(GalleryTab tab);

// The Gallery screen.
void galleryScreen(void);

#endif /* GALLERY_H */
