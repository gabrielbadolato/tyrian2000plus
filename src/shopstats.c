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
#include "shopstats.h"

#include "episodes.h"

#include <stdlib.h>

// the game runs at about 35 frames per second at normal speed
#define FRAMES_PER_SECOND 35

// an enemy is about this many pixels tall: a shot that goes through enemies
// hits again every frame it is still over one
#define ENEMY_SIZE 20

// frames a piercing shot of pattern i stays over an enemy
static uint pierce_frames(const JE_WeaponType *w, uint i)
{
	int speed = MAX(abs(w->sx[i]), abs(w->sy[i]));
	if (w->sy[i] > 90)  // special movement codes, not a speed
		speed = abs(w->sx[i]);
	uint frames = speed > 0 ? (ENEMY_SIZE + speed - 1) / speed : ENEMY_SIZE;
	const uint lifetime = w->del[i] == 121 ? 255 : w->del[i];
	if (frames > lifetime)
		frames = lifetime;
	return MAX(frames, 1u);
}

// damage of shot pattern i, as the collision code in tyrian2.c applies it
static uint shot_damage(const JE_WeaponType *w, uint i, bool follow_chain)
{
	const uint attack = w->attack[i];

	if (w->sg[i] == 0)  // a pattern without a sprite fires nothing
		return 0;
	if (attack == 99)  // freezes the enemy, no damage
		return 0;
	if (attack > 99 && attack < 250)  // chain reaction: on hit, fires another weapon
	{
		uint damage = 1;
		const uint chain = attack - 100;
		if (follow_chain && chain > 0 && chain <= WEAP_NUM)
		{
			const JE_WeaponType *c = &weapons[chain];
			const uint positions = c->max == 0 ? 1 : MIN(c->max, 8u);
			for (uint j = 0; j < positions && j < c->multi; ++j)
				damage += shot_damage(c, j, false);
		}
		return damage;
	}
	if (attack >= 250)  // goes through enemies
		return (attack - 250) * pierce_frames(w, i);
	return attack;
}

uint shop_weapon_damage(uint wpNum)
{
	if (wpNum == 0 || wpNum > WEAP_NUM)
		return 0;

	const JE_WeaponType *w = &weapons[wpNum];

	// each volley fires "multi" shots, cycling through the first "max"
	// shot patterns (player_shot_create), so on average a shot does the
	// mean damage of those patterns
	const uint positions = w->max == 0 ? 1 : MIN(w->max, 8u);
	uint sum = 0;
	for (uint i = 0; i < positions; ++i)
		sum += shot_damage(w, i, true);

	// a volley every shotrepeat + 1 frames
	const unsigned long per_second = (unsigned long)sum * w->multi * FRAMES_PER_SECOND;
	const unsigned long divisor = (unsigned long)positions * (w->shotrepeat + 1u);
	return (uint)((per_second + divisor / 2) / divisor);
}

uint shop_item_stat(uint menu, uint item, uint power)
{
	if (power < 1)
		power = 1;
	else if (power > 11)
		power = 11;

	switch (menu)
	{
	case 2:  // ship
		return item <= SHIP_NUM ? ships[item].dmg : 0;
	case 3:  // front and rear weapon
	case 4:
		return (item > 0 && item <= PORT_NUM) ? shop_weapon_damage(weaponPort[item].op[0][power - 1]) : 0;
	case 5:  // shield
		return item <= SHIELD_NUM ? shields[item].mpwr : 0;
	case 6:  // generator
		return item <= POWER_NUM ? powerSys[item].power : 0;
	case 7:  // sidekicks
	case 8:
		return (item > 0 && item <= OPTION_NUM) ? shop_weapon_damage(options[item].wpnum) : 0;
	}
	return 0;
}

const char *shop_stat_format(uint menu)
{
	switch (menu)
	{
	case 2:
		return "Armor %u";
	case 5:
		return "Shield %u";
	case 6:
		return "Power %u";
	}
	return "Damage %u";
}
