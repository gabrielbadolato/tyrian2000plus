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

// The Gallery (Setup > "Gallery..."): the ships, weapons, sidekicks and other
// items the player has come across, and the enemies met in the levels, drawn
// with the game's own pictures.  What has been seen is kept in gallery.cfg in
// the user folder, apart from tyrian.sav; recording it only reads the game.

// Reads gallery.cfg (at start).
void gallery_load(void);

// The level loop (src/tyrian2.c): the start and end of a level and each of
// its frames, an enemy made in slot i (0..99) and an enemy destroyed.
void gallery_level_start(void);
void gallery_level_frame(void);
void gallery_level_end(void);
void gallery_enemy_made(unsigned int slot);
void gallery_enemy_killed(unsigned int slot);

// The items of the ship's upgrade menu (itemAvail) were loaded.
void gallery_shop_items(void);

// Tests: record also with constant play (-c), which levels normally do not.
extern bool gallery_test_record;

// How many entries the gallery has seen, of each kind (tests).
typedef enum
{
	GALLERY_SHIPS,
	GALLERY_WEAPONS,
	GALLERY_SIDEKICKS,
	GALLERY_OTHER,
	GALLERY_ENEMIES,
	GALLERY_TABS
} GalleryTab;
unsigned int gallery_seen_count(GalleryTab tab);

// The Gallery screen.
void galleryScreen(void);

#endif /* GALLERY_H */
