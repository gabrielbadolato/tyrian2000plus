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
#ifndef SHOPSTATS_H
#define SHOPSTATS_H

#include "opentyr.h"

// Numbers the ship's upgrade menu shows next to each item so it can be
// compared with what the ship has.  They come from the game's own item data,
// which is not changed.

// Damage a weapon does per second, at normal game speed, with the weapon
// number used for one power level (weaponPort[].op or options[].wpnum).
// It is an estimate: homing, piercing and similar effects are not counted.
uint shop_weapon_damage(uint wpNum);

// The number shown for an item of an upgrade menu row (curSel of
// MENU_UPGRADES: 2 ship, 3 front, 4 rear, 5 shield, 6 generator, 7-8
// sidekicks).  power is the weapon's power level (1-11); others ignore it.
// Returns 0 when there is nothing to compare (e.g. "None").
uint shop_item_stat(uint menu, uint item, uint power);

// printf format for that number ("Damage %u", "Armor %u"...), to be passed
// through tr().
const char *shop_stat_format(uint menu);

#endif /* SHOPSTATS_H */
