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
 * The Gallery: what the player has seen of the game, drawn with the game's
 * own pictures.
 *
 * Items: the ones the ship's upgrade menu offered (the "]I" lists of
 * levelsN.dat, read again by the gallery to know which items there are) and
 * the ones the players had.  Enemies: a fixed list of the game's enemies
 * (src/gallery_catalog.h, made by tests/make_gallery_catalog.py from every
 * level played by the autopilot), each one by how it looks, with the
 * enemyDat index of its pieces.  What the levels make is put together in
 * things (the pieces of a boss or of a building) and each thing marks the
 * enemies of the list it is made of; one the list does not have is kept as
 * it looks.  All of it is kept in gallery.cfg; nothing here changes the game.
 */
#include "gallery.h"
#include "gallery_catalog.h"

#include "backgrnd.h"
#include "config.h"
#include "config_file.h"
#include "demo.h"
#include "episodes.h"
#include "file.h"
#include "font.h"
#include "fonthand.h"
#include "helptext.h"
#include "joystick.h"
#include "keyboard.h"
#include "lang.h"
#include "levelrec.h"
#include "logging.h"
#include "lvlmast.h"
#include "mainint.h"
#include "menus.h"
#include "mouse.h"
#include "nortsong.h"
#include "palette.h"
#include "params.h"
#include "picload.h"
#include "player.h"
#include "shopstats.h"
#include "shots.h"
#include "smooth.h"
#include "sndmast.h"
#include "sprite.h"
#include "stats.h"
#include "tyrian2.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"

#include "SDL.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool gallery_test_record = false;
bool gallery_test_no_list = false;
FILE *gallery_dump = NULL;

static const char *const galleryFilename = "gallery.cfg";

/* -------- what has been seen -------- */

static bool seenShip[SHIP_NUM + 1];
static bool seenWeapon[PORT_NUM + 1];
static bool seenSidekick[OPTION_NUM + 1];
static bool seenShield[SHIELD_NUM + 1];
static bool seenGenerator[POWER_NUM + 1];
static bool seenSpecial[SPECIAL_NUM + 1];

#define MAX_PARTS 32
#define MAX_ENEMIES 2000

typedef struct
{
	Uint8 bank;   // sprite bank (shapeFile[bank - 1])
	Uint8 size;   // 0: one sprite, 1: 2x2 sprites
	Uint16 gr;    // first sprite
	Sint16 x, y;  // from the first piece
	Uint16 type;  // enemyDat index, armor and slot, while it is recorded (not kept)
	Uint8 armor;
	Uint8 slot;
	Uint16 anim;  // its animation (catalogAnims; 0: none), for one of the list
} Part;

typedef struct
{
	unsigned int parts;
	Part part[MAX_PARTS];
	unsigned int armor;    // all the pieces; 255 a piece that cannot be destroyed
	bool indestructible;
	unsigned int episode;
	char level[11];
	unsigned int killed;
	bool listed;  // one of the game's enemies (src/gallery_catalog.h)
	bool seen;
} SeenEnemy;

static SeenEnemy *enemies;  // MAX_ENEMIES
static unsigned int enemyCount;

static bool dirty;

// the level being played
static bool recording;
static Uint8 pending[100];  // enemies made since the last look, by slot
static unsigned int pendingCount;

// A thing being met: an enemy, or the pieces of a boss or of a building,
// which can come over several frames (a building scrolls in row by row).
// It is recorded when all of its pieces are gone.
typedef struct
{
	bool used;
	bool killed;     // its last piece was destroyed
	SeenEnemy look;  // parts where they are in the thing
	unsigned int made;  // the frame it came (for gallery_dump)
	long scroll;        // how far its map layer had scrolled then (for gallery_dump)
} Thing;

static unsigned int frameCount;
static long groundScroll, topScroll;  // the map layers, since the level started

// How far the map layer of the enemy in `slot` has scrolled: where it is on
// the map is its y less this (0 in the sky).
static long layerScroll(unsigned int slot)
{
	return slot < 25 ? 0 : (slot >= 50 && slot < 75) ? topScroll : groundScroll;
}

static Thing things[100];
static Uint8 slotThing[100];  // the thing of the enemy in each slot + 1, or 0
static bool slotAlone[100];  // made by another enemy: a thing of its own
static int slotX[100], slotY[100];  // where the enemy is in its thing

static void markItem(bool *seen, size_t count, unsigned int id)
{
	if (id > 0 && id < count && !seen[id])
	{
		seen[id] = true;
		dirty = true;
	}
}

#define MARK(seen, id) markItem(seen, COUNTOF(seen), id)

static void markPlayerItems(const PlayerItems *items)
{
	MARK(seenShip, items->ship);
	MARK(seenWeapon, items->weapon[0].id);
	MARK(seenWeapon, items->weapon[1].id);
	MARK(seenSidekick, items->sidekick[0]);
	MARK(seenSidekick, items->sidekick[1]);
	MARK(seenShield, items->shield);
	MARK(seenGenerator, items->generator);
	MARK(seenSpecial, items->special);
}

static bool partsEqual(const SeenEnemy *a, const SeenEnemy *b)
{
	if (a->parts != b->parts)
		return false;
	for (unsigned int i = 0; i < a->parts; ++i)
	{
		const Part *p = &a->part[i], *q = &b->part[i];
		if (p->bank != q->bank || p->size != q->size || p->gr != q->gr || p->x != q->x || p->y != q->y)
			return false;
	}
	return true;
}

// Whether all of a's parts are in b, which has more: a piece of b, such as
// the first rows of a building or one turret on it.
static bool pieceOf(const SeenEnemy *a, const SeenEnemy *b)
{
	if (a->parts == 0 || a->parts >= b->parts)
		return false;

	const Part *first = &a->part[0];
	for (unsigned int j = 0; j < b->parts; ++j)
	{
		const Part *q = &b->part[j];
		if (q->bank != first->bank || q->gr != first->gr || q->size != first->size)
			continue;

		const int dx = q->x - first->x, dy = q->y - first->y;
		bool all = true;
		for (unsigned int i = 1; i < a->parts && all; ++i)
		{
			const Part *p = &a->part[i];
			all = false;
			for (unsigned int k = 0; k < b->parts; ++k)
			{
				const Part *r = &b->part[k];
				if (r->bank == p->bank && r->gr == p->gr && r->size == p->size && r->x == p->x + dx && r->y == p->y + dy)
				{
					all = true;
					break;
				}
			}
		}
		if (all)
			return true;
	}
	return false;
}

// The entry that `look` is a piece of, or -1.
static int findWhole(const SeenEnemy *look)
{
	for (unsigned int i = 0; i < enemyCount; ++i)
		if (pieceOf(look, &enemies[i]))
			return (int)i;
	return -1;
}

static int findEnemy(const SeenEnemy *look)
{
	for (unsigned int i = 0; i < enemyCount; ++i)
		if (partsEqual(&enemies[i], look))
			return (int)i;
	return -1;
}

/* -------- drawing sprites into a canvas -------- */

// A sprite2 is 12x14; the drawing code goes to the next row by `pitch - 12`,
// so the canvas is as wide as the game's screen.
enum { CANVAS_W = 320, CANVAS_H = 240 };

typedef struct
{
	Uint8 pixel[CANVAS_W * CANVAS_H];
	bool set[CANVAS_W * CANVAS_H];
	int x1, y1, x2, y2;  // what was drawn (x2, y2 past the end)
} Canvas;

static Canvas canvas;

static void canvasClear(void)
{
	memset(canvas.set, 0, sizeof(canvas.set));
	canvas.x1 = canvas.y1 = INT32_MAX;
	canvas.x2 = canvas.y2 = INT32_MIN;
}

// Draws sprite `index` of `sheet` at (x, y), as blit_sprite2 does; false if
// the sheet does not have it.  Counts the pixels drawn in *drawn.
static bool canvasSprite2(const Sprite2_array *sheet, unsigned int index, int x, int y, unsigned int *drawn)
{
	if (sheet == NULL || sheet->data == NULL || sheet->size < 2 || index == 0)
		return false;

	const Uint16 first = SDL_SwapLE16(((const Uint16 *)sheet->data)[0]);
	if (index - 1 >= first / 2u || first > sheet->size)
		return false;

	size_t at = SDL_SwapLE16(((const Uint16 *)sheet->data)[index - 1]);
	long pos = (long)y * CANVAS_W + x;
	const long end = (long)CANVAS_W * CANVAS_H;

	for (; at < sheet->size && sheet->data[at] != 0x0f; ++at)
	{
		pos += sheet->data[at] & 0x0f;
		unsigned int count = sheet->data[at] >> 4;

		if (count == 0)
		{
			pos += CANVAS_W - 12;
			continue;
		}
		while (count--)
		{
			if (++at >= sheet->size)
				return true;
			if (pos >= 0 && pos < end)
			{
				canvas.pixel[pos] = sheet->data[at];
				canvas.set[pos] = true;
				const int px = (int)(pos % CANVAS_W), py = (int)(pos / CANVAS_W);
				canvas.x1 = MIN(canvas.x1, px);
				canvas.y1 = MIN(canvas.y1, py);
				canvas.x2 = MAX(canvas.x2, px + 1);
				canvas.y2 = MAX(canvas.y2, py + 1);
				if (drawn != NULL)
					++*drawn;
			}
			++pos;
		}
	}
	return true;
}

static void canvasSprite2x2(const Sprite2_array *sheet, unsigned int index, int x, int y, unsigned int *drawn)
{
	canvasSprite2(sheet, index, x, y, drawn);
	canvasSprite2(sheet, index + 1, x + 12, y, drawn);
	canvasSprite2(sheet, index + 19, x, y + 14, drawn);
	canvasSprite2(sheet, index + 20, x + 12, y + 14, drawn);
}

// how an enemy piece draws (blit_enemy in tyrian2.c), at (x, y)
static unsigned int canvasPiece(const Sprite2_array *sheet, unsigned int gr, unsigned int size, int x, int y)
{
	unsigned int drawn = 0;
	if (size == 1)
	{
		canvasSprite2(sheet, gr, x - 6, y - 7, &drawn);
		canvasSprite2(sheet, gr + 1, x + 6, y - 7, &drawn);
		canvasSprite2(sheet, gr + 19, x - 6, y + 7, &drawn);
		canvasSprite2(sheet, gr + 20, x + 6, y + 7, &drawn);
	}
	else
	{
		canvasSprite2(sheet, gr, x, y, &drawn);
	}
	return drawn;
}

/* -------- recording in the levels -------- */

static bool recordingAllowed(void)
{
	return !playDemo && !levelrec_watching() && ((!constantPlay && !constantDie) || gallery_test_record);
}

void gallery_level_start(void)
{
	recording = recordingAllowed();
	pendingCount = 0;
	memset(things, 0, sizeof(things));
	memset(slotThing, 0, sizeof(slotThing));
	memset(slotAlone, 0, sizeof(slotAlone));
	groundScroll = topScroll = 0;

	if (!recording)
		return;

	markPlayerItems(&player[0].items);
	if (twoPlayerMode)
		markPlayerItems(&player[1].items);
}

void gallery_enemy_made(unsigned int slot)
{
	if (!recording || slot >= COUNTOF(pending))
		return;

	slotThing[slot] = 0;  // what was there before is gone
	slotAlone[slot] = false;
	for (unsigned int i = 0; i < pendingCount; ++i)
		if (pending[i] == slot)
			return;
	pending[pendingCount++] = (Uint8)slot;
}

void gallery_enemy_launched(unsigned int slot)
{
	gallery_enemy_made(slot);
	if (slot < COUNTOF(slotAlone))
		slotAlone[slot] = true;
}

// The bank of the sprite sheet an enemy draws from (0: not one of the
// banks of the levels, such as coins and gems).
static Uint8 enemyBank(unsigned int slot)
{
	for (unsigned int i = 0; i < COUNTOF(enemySpriteSheets); ++i)
		if (enemy[slot].sprite2s == &enemySpriteSheets[i])
			return enemySpriteSheetIds[i];
	return 0;
}

// The first picture of the enemy that is drawn (999: nothing).
static unsigned int enemyGraphic(unsigned int slot)
{
	const struct JE_SingleEnemyType *e = &enemy[slot];
	const unsigned int frames = MAX(1u, MIN((unsigned int)e->ani, 20u));
	for (unsigned int i = 0; i < frames; ++i)
		if (e->egr[i] != 999 && e->egr[i] != 0)
			return e->egr[i];
	return 999;
}

// Whether the piece in `slot` shows anything.
static bool pieceVisible(unsigned int slot, Uint8 bank, unsigned int gr)
{
	if (bank == 0 || gr == 999)
		return false;

	canvasClear();
	return canvasPiece(enemy[slot].sprite2s, gr, enemy[slot].size, 100, 100) > 0;
}

// The area a piece covers, as in JE_drawEnemy
static void pieceBox(unsigned int slot, int *x1, int *y1, int *x2, int *y2)
{
	const int x = enemy[slot].ex, y = enemy[slot].ey;
	if (enemy[slot].size == 1)
	{
		*x1 = x - 6; *y1 = y - 7; *x2 = x + 18; *y2 = y + 21;
	}
	else
	{
		*x1 = x; *y1 = y; *x2 = x + 12; *y2 = y + 14;
	}
}

static bool piecesTouch(unsigned int a, unsigned int b)
{
	int ax1, ay1, ax2, ay2, bx1, by1, bx2, by2;
	pieceBox(a, &ax1, &ay1, &ax2, &ay2);
	pieceBox(b, &bx1, &by1, &bx2, &by2);
	const int gap = 2;
	return ax1 < bx2 + gap && bx1 < ax2 + gap && ay1 < by2 + gap && by1 < ay2 + gap;
}

// Enemies on the map: ground (JE_drawEnemy(50) and (100)) and top
// (JE_drawEnemy(75)), such as the pieces of a building.
static bool mapSlot(unsigned int slot)
{
	return slot >= 25;
}

// Whether two enemies made in the same frame are pieces of one thing: a boss
// (linked) or a building on the map, made of enemies side by side.
static bool samePiece(unsigned int a, unsigned int b)
{
	if (slotAlone[a] || slotAlone[b] || !piecesTouch(a, b))
		return false;
	if (enemy[a].linknum != 0 && enemy[a].linknum == enemy[b].linknum)
		return true;
	return mapSlot(a) && mapSlot(b);
}

static int comparePart(const void *a, const void *b)
{
	const Part *p = a, *q = b;
	if (p->y != q->y)
		return p->y - q->y;
	if (p->x != q->x)
		return p->x - q->x;
	if (p->bank != q->bank)
		return p->bank - q->bank;
	if (p->gr != q->gr)
		return p->gr - q->gr;
	return p->size - q->size;
}

// Whether the enemy in `slot` belongs to a thing and is still there.
static unsigned int catalogMatch(const SeenEnemy *look, unsigned int *match, unsigned int max);
static bool catalogHas(unsigned int entry, Uint16 type);

static bool slotInThing(unsigned int slot)
{
	return slotThing[slot] != 0 && enemyAvail[slot] != 1;
}

// One more of an enemy met (destroyed or not).
static void seeEnemy(SeenEnemy *e, const char *level, unsigned int armor, bool indestructible, bool killed)
{
	if (!e->seen)
	{
		// where this player met it (the list says where it comes first)
		e->seen = true;
		e->episode = episodeNum;
		snprintf(e->level, sizeof(e->level), "%s", level);
		dirty = true;
	}
	if (armor > e->armor || (indestructible && !e->indestructible))
	{
		e->armor = MAX(e->armor, armor);
		e->indestructible |= indestructible;
		dirty = true;
	}
	if (killed)
	{
		++e->killed;
		dirty = true;
	}
}

// Records a thing whose pieces are all gone.
static void finishThing(unsigned int t)
{
	Thing *thing = &things[t];
	if (!thing->used)
		return;
	thing->used = false;

	for (unsigned int i = 0; i < COUNTOF(slotThing); ++i)
		if (slotThing[i] == t + 1)
			slotThing[i] = 0;

	SeenEnemy look = thing->look;
	if (look.parts == 0)
		return;  // nothing that shows, or not a target

	// the same however it is placed: from the top left piece, in order
	int minX = look.part[0].x, minY = look.part[0].y;
	for (unsigned int i = 1; i < look.parts; ++i)
	{
		minX = MIN(minX, look.part[i].x);
		minY = MIN(minY, look.part[i].y);
	}
	for (unsigned int i = 0; i < look.parts; ++i)
	{
		look.part[i].x -= (Sint16)minX;
		look.part[i].y -= (Sint16)minY;
	}
	qsort(look.part, look.parts, sizeof(Part), comparePart);

	if (gallery_dump != NULL)
	{
		fprintf(gallery_dump, "%u\t%.10s\t%d", episodeNum, levelName, thing->killed);
		for (unsigned int i = 0; i < look.parts; ++i)
		{
			const Part *p = &look.part[i];
			fprintf(gallery_dump, "\t%u:%u:%u:%d:%d:%u:%u", p->bank, p->gr, p->size, p->x, p->y, p->type, p->slot);
		}
		// when it came and where (the things that come together, such as a
		// boss and its arms when they are not linked)
		fprintf(gallery_dump, "\t@%u:%d:%d:%ld\n", thing->made, minX, minY, minY - thing->scroll);
	}

	char level[11];
	snprintf(level, sizeof(level), "%s", levelName);
	for (size_t n = strlen(level); n > 0 && level[n - 1] == ' '; --n)
		level[n - 1] = '\0';

	// the enemies of the list it is (two buildings side by side are two)
	unsigned int match[MAX_PARTS];
	const unsigned int matches = catalogMatch(&look, match, COUNTOF(match));
	for (unsigned int m = 0; m < matches; ++m)
	{
		SeenEnemy *e = &enemies[match[m]];
		unsigned int armor = 0;
		bool indestructible = false;
		for (unsigned int i = 0; i < look.parts; ++i)
		{
			if (!catalogHas(match[m], look.part[i].type))
				continue;
			if (look.part[i].armor == 255)
				indestructible = true;
			else
				armor += look.part[i].armor;
		}
		seeEnemy(e, level, armor, indestructible, thing->killed);
	}
	if (matches > 0)
		return;

	// left out of the list on purpose (a dot that is hardly seen): nothing
	if (!gallery_test_no_list)
	{
		bool hidden = true;
		for (unsigned int i = 0; i < look.parts && hidden; ++i)
			hidden = catalogHas(CATALOG_HIDDEN, look.part[i].type);
		if (hidden)
			return;
	}

	// not in the list: as it looks
	int entry = findEnemy(&look);
	if (entry < 0)
		entry = findWhole(&look);  // seen whole before: that one
	if (entry < 0)
	{
		if (enemyCount >= MAX_ENEMIES)
			return;
		entry = (int)enemyCount++;
		look.episode = episodeNum;
		look.killed = 0;
		look.listed = false;
		look.seen = false;
		enemies[entry] = look;
	}
	seeEnemy(&enemies[entry], level, look.armor, look.indestructible, thing->killed);
}

// Records the things none of whose pieces are left.
static void finishGoneThings(void)
{
	bool left[COUNTOF(things)] = { false };
	for (unsigned int i = 0; i < COUNTOF(slotThing); ++i)
		if (slotInThing(i))
			left[slotThing[i] - 1] = true;

	for (unsigned int t = 0; t < COUNTOF(things); ++t)
		if (things[t].used && !left[t])
			finishThing(t);
}

// Adds the enemy in `slot` to thing t, at x, y.
static void addToThing(unsigned int t, unsigned int slot, int x, int y)
{
	slotThing[slot] = (Uint8)(t + 1);
	slotX[slot] = x;
	slotY[slot] = y;

	SeenEnemy *look = &things[t].look;
	if (look->parts >= MAX_PARTS)
		return;

	const Uint8 bank = enemyBank(slot);
	const unsigned int gr = enemyGraphic(slot);
	const unsigned int armor = enemyDat[enemy[slot].enemytype].armor;
	if (armor == 0)
		return;  // not a target: scenery
	if (!pieceVisible(slot, bank, gr))
		return;  // such as the invisible enemy that holds a boss's map

	Part *part = &look->part[look->parts++];
	part->bank = bank;
	part->size = enemy[slot].size;
	part->gr = (Uint16)gr;
	part->x = (Sint16)x;
	part->y = (Sint16)y;
	part->type = enemy[slot].enemytype;
	part->armor = (Uint8)armor;
	part->slot = (Uint8)slot;

	if (armor == 255)
		look->indestructible = true;
	else
		look->armor += armor;
}

// Moves the pieces of thing `from` into thing `to`; `fromSlot` and `toSlot`
// are pieces of each, still there, which give where one is from the other.
static void mergeThings(unsigned int to, unsigned int toSlot, unsigned int from, unsigned int fromSlot)
{
	const int dx = (slotX[toSlot] - enemy[toSlot].ex) - (slotX[fromSlot] - enemy[fromSlot].ex);
	const int dy = (slotY[toSlot] - enemy[toSlot].ey) - (slotY[fromSlot] - enemy[fromSlot].ey);

	SeenEnemy *look = &things[to].look;
	const SeenEnemy *other = &things[from].look;
	for (unsigned int i = 0; i < other->parts && look->parts < MAX_PARTS; ++i)
	{
		Part part = other->part[i];
		part.x = (Sint16)(part.x + dx);
		part.y = (Sint16)(part.y + dy);
		look->part[look->parts++] = part;
	}
	look->armor += other->armor;
	look->indestructible |= other->indestructible;

	for (unsigned int i = 0; i < COUNTOF(slotThing); ++i)
	{
		if (slotThing[i] == from + 1)
		{
			slotThing[i] = (Uint8)(to + 1);
			slotX[i] += dx;
			slotY[i] += dy;
		}
	}
	memset(&things[from], 0, sizeof(things[from]));
}

// Looks at the enemies made since the last look: the pieces made together
// that touch are one thing, and pieces that touch a thing already there
// (the next rows of a building) join it.
static void lookAtPending(void)
{
	bool done[100] = { false };

	for (unsigned int p = 0; p < pendingCount; ++p)
	{
		const unsigned int first = pending[p];
		if (done[first])
			continue;
		done[first] = true;

		// the group: the pieces made with it that touch it, one by one
		unsigned int group[100], groupCount = 0;
		group[groupCount++] = first;
		for (unsigned int g = 0; g < groupCount; ++g)
		{
			for (unsigned int q = 0; q < pendingCount; ++q)
			{
				const unsigned int other = pending[q];
				if (!done[other] && samePiece(group[g], other))
				{
					done[other] = true;
					group[groupCount++] = other;
				}
			}
		}

		// the things already there that it touches
		int thing = -1;
		unsigned int anchor = 0;
		for (unsigned int g = 0; g < groupCount; ++g)
		{
			for (unsigned int i = 0; i < COUNTOF(slotThing); ++i)
			{
				// only on the map: linked enemies that come one by one are a formation
				if (!slotInThing(i) || slotThing[i] == thing + 1 || slotAlone[group[g]] || slotAlone[i] ||
				    !mapSlot(group[g]) || !mapSlot(i) || !piecesTouch(group[g], i))
					continue;
				if (thing < 0)
				{
					thing = slotThing[i] - 1;
					anchor = i;
				}
				else
				{
					mergeThings((unsigned int)thing, anchor, slotThing[i] - 1u, i);
				}
			}
		}

		int dx = 0, dy = 0;
		if (thing >= 0)
		{
			dx = slotX[anchor] - enemy[anchor].ex;
			dy = slotY[anchor] - enemy[anchor].ey;
		}
		else
		{
			for (unsigned int t = 0; t < COUNTOF(things); ++t)
			{
				if (!things[t].used)
				{
					thing = (int)t;
					break;
				}
			}
			if (thing < 0)
				continue;  // cannot happen: a thing has a piece in a slot
			memset(&things[thing], 0, sizeof(things[thing]));
			things[thing].used = true;
			things[thing].made = frameCount;
			things[thing].scroll = layerScroll(group[0]);
		}

		for (unsigned int g = 0; g < groupCount; ++g)
			addToThing((unsigned int)thing, group[g], enemy[group[g]].ex + dx, enemy[group[g]].ey + dy);
	}

	pendingCount = 0;
}

void gallery_level_frame(void)
{
	if (recording)
	{
		// (as JE_drawEnemy moves them: slots 50-74 with the top layer)
		groundScroll += backMove;
		topScroll += backMove3;
		lookAtPending();
		finishGoneThings();
	}
	++frameCount;
}

void gallery_enemy_killed(unsigned int slot)
{
	if (!recording || slot >= COUNTOF(slotThing))
		return;

	lookAtPending();  // made in this frame

	if (slotThing[slot] == 0)
		return;
	const unsigned int t = slotThing[slot] - 1u;
	slotThing[slot] = 0;

	// a boss or a building counts when its last piece goes
	for (unsigned int i = 0; i < COUNTOF(slotThing); ++i)
		if (slotThing[i] == t + 1 && slotInThing(i))
			return;

	things[t].killed = true;
	finishThing(t);
}

void gallery_shop_items(void)
{
	if (playDemo)
		return;

	// rows of itemAvail: ship, front, rear, generator, (engine), sidekicks, (armor), shield
	for (int i = 0; i < itemAvailMax[0]; ++i)
		MARK(seenShip, itemAvail[0][i]);
	for (int row = 1; row <= 2; ++row)
		for (int i = 0; i < itemAvailMax[row]; ++i)
			MARK(seenWeapon, itemAvail[row][i]);
	for (int i = 0; i < itemAvailMax[3]; ++i)
		MARK(seenGenerator, itemAvail[3][i]);
	for (int row = 5; row <= 6; ++row)
		for (int i = 0; i < itemAvailMax[row]; ++i)
			MARK(seenSidekick, itemAvail[row][i]);
	for (int i = 0; i < itemAvailMax[8]; ++i)
		MARK(seenShield, itemAvail[8][i]);
}

/* -------- gallery.cfg -------- */

static void loadIds(const ConfigSection *section, const char *key, bool *seen, size_t count)
{
	const char *value;
	if (!config_get_string_option(section, key, &value))
		return;

	const char *s = value;
	while (*s != '\0')
	{
		char *end;
		const unsigned long id = strtoul(s, &end, 10);
		if (end == s)
			break;
		if (id > 0 && id < count)
			seen[id] = true;
		s = end;
		while (*s == ' ' || *s == ',')
			++s;
	}
}

static void saveIds(ConfigSection *section, const char *key, const bool *seen, size_t count)
{
	char buffer[512] = "";
	size_t len = 0;
	for (size_t id = 1; id < count && len + 8 < sizeof(buffer); ++id)
		if (seen[id])
			len += (size_t)snprintf(buffer + len, sizeof(buffer) - len, len > 0 ? " %u" : "%u", (unsigned int)id);
	config_set_string_option(section, key, buffer);
}

// The list of the game's enemies first, none seen.
static bool ensureEnemies(void)
{
	if (enemies == NULL)
		enemies = calloc(MAX_ENEMIES, sizeof(*enemies));
	if (enemies == NULL)
		return false;

	if (enemyCount == 0 && !gallery_test_no_list)
	{
		for (unsigned int i = 0; i < COUNTOF(catalogEnemies) && i < MAX_ENEMIES; ++i)
		{
			const CatalogEnemy *c = &catalogEnemies[i];
			SeenEnemy *e = &enemies[i];
			memset(e, 0, sizeof(*e));
			for (unsigned int p = 0; p < c->parts && p < MAX_PARTS; ++p)
			{
				const CatalogPart *cp = &catalogParts[c->first + p];
				e->part[e->parts++] = (Part){ .bank = cp->bank, .size = cp->size, .gr = cp->gr, .x = cp->x, .y = cp->y, .anim = cp->anim };
			}
			e->episode = c->episode;
			e->listed = true;
		}
		enemyCount = MIN(COUNTOF(catalogEnemies), (size_t)MAX_ENEMIES);
	}
	return true;
}

// The entries of catalogTypes for an enemyDat index of this episode's data
static unsigned int catalogTypeFirst(Uint16 type)
{
	const unsigned int dataset = episodeNum <= 3 ? 1 : episodeNum;
	unsigned int lo = 0, hi = COUNTOF(catalogTypes);
	while (lo < hi)
	{
		const unsigned int mid = (lo + hi) / 2;
		const CatalogType *t = &catalogTypes[mid];
		if (t->dataset < dataset || (t->dataset == dataset && t->type < type))
			lo = mid + 1;
		else
			hi = mid;
	}
	return lo;
}

static bool catalogTypeIs(unsigned int k, Uint16 type)
{
	const unsigned int dataset = episodeNum <= 3 ? 1 : episodeNum;
	return k < COUNTOF(catalogTypes) && catalogTypes[k].dataset == dataset && catalogTypes[k].type == type;
}

// Whether the enemy `entry` of the list has pieces of enemyDat index `type`
static bool catalogHas(unsigned int entry, Uint16 type)
{
	for (unsigned int k = catalogTypeFirst(type); catalogTypeIs(k, type); ++k)
		if (catalogTypes[k].enemy == entry)
			return true;
	return false;
}

// The enemies of the list that a thing seen is, by the enemyDat index of its
// pieces: for each piece, the enemy that has the most pieces of the thing
// (the smallest if more do).  Returns how many.
static unsigned int catalogMatch(const SeenEnemy *look, unsigned int *match, unsigned int max)
{
	if (gallery_test_no_list)
		return 0;

	unsigned int candidate[64], score[64], candidates = 0;
	for (unsigned int i = 0; i < look->parts; ++i)
	{
		for (unsigned int k = catalogTypeFirst(look->part[i].type); catalogTypeIs(k, look->part[i].type); ++k)
		{
			const unsigned int e = catalogTypes[k].enemy;
			if (e == CATALOG_HIDDEN)
				continue;
			unsigned int c = 0;
			while (c < candidates && candidate[c] != e)
				++c;
			if (c == candidates)
			{
				if (candidates == COUNTOF(candidate))
					continue;
				candidate[candidates] = e;
				score[candidates++] = 0;
			}
			++score[c];
		}
	}

	unsigned int matches = 0;
	for (unsigned int i = 0; i < look->parts; ++i)
	{
		int best = -1;
		for (unsigned int c = 0; c < candidates; ++c)
		{
			if (!catalogHas(candidate[c], look->part[i].type))
				continue;
			if (best < 0 || score[c] > score[best] ||
			    (score[c] == score[best] && catalogEnemies[candidate[c]].parts < catalogEnemies[candidate[best]].parts))
				best = (int)c;
		}
		if (best < 0)
			continue;
		unsigned int m = 0;
		while (m < matches && match[m] != candidate[best])
			++m;
		if (m == matches && matches < max)
			match[matches++] = candidate[best];
	}
	return matches;
}

void gallery_load(void)
{
	memset(seenShip, 0, sizeof(seenShip));
	memset(seenWeapon, 0, sizeof(seenWeapon));
	memset(seenSidekick, 0, sizeof(seenSidekick));
	memset(seenShield, 0, sizeof(seenShield));
	memset(seenGenerator, 0, sizeof(seenGenerator));
	memset(seenSpecial, 0, sizeof(seenSpecial));
	enemyCount = 0;
	dirty = false;

	if (!ensureEnemies())
		return;  // the list, none seen

	File file = userFileOpen(galleryFilename, "r");
	if (file.error)
		return;  // nothing seen yet

	Config config;
	config_init(&config);

	const bool success = config_parse(&config, file.f);
	fileClose(&file);

	if (!success)
	{
		logError("Failed to parse '%s'.", galleryFilename);
		config_deinit(&config);
		return;
	}

	const ConfigSection *section = config_find_section(&config, "items", NULL);
	if (section != NULL)
	{
		loadIds(section, "ships", seenShip, COUNTOF(seenShip));
		loadIds(section, "weapons", seenWeapon, COUNTOF(seenWeapon));
		loadIds(section, "sidekicks", seenSidekick, COUNTOF(seenSidekick));
		loadIds(section, "shields", seenShield, COUNTOF(seenShield));
		loadIds(section, "generators", seenGenerator, COUNTOF(seenGenerator));
		loadIds(section, "specials", seenSpecial, COUNTOF(seenSpecial));
	}

	for (unsigned int i = 0; i < config.sections_count && enemyCount < MAX_ENEMIES; ++i)
	{
		section = &config.sections[i];
		if (strcmp(config_string_to_cstr(&section->type), "enemy") != 0)
			continue;

		const char *parts;
		if (!config_get_string_option(section, "parts", &parts))
			continue;

		SeenEnemy *e = &enemies[enemyCount];
		memset(e, 0, sizeof(*e));
		e->seen = true;

		// "bank:sprite:size:x:y" for each piece
		const char *s = parts;
		while (*s != '\0' && e->parts < MAX_PARTS)
		{
			int bank, gr, size, x, y, n = 0;
			if (sscanf(s, "%d:%d:%d:%d:%d%n", &bank, &gr, &size, &x, &y, &n) != 5 || n == 0)
				break;
			if (bank >= 1 && bank <= (int)COUNTOF(shapeFile) && gr > 0 && gr < 999 && (size == 0 || size == 1) &&
			    abs(x) < 1000 && abs(y) < 1000)
			{
				e->part[e->parts++] = (Part){ .bank = (Uint8)bank, .size = (Uint8)size, .gr = (Uint16)gr, .x = (Sint16)x, .y = (Sint16)y };
			}
			s += n;
			while (*s == ' ')
				++s;
		}
		if (e->parts == 0)
			continue;

		unsigned int value;
		if (config_get_uint_option(section, "armor", &value))
			e->armor = value;
		bool indestructible;
		if (config_get_bool_option(section, "indestructible", &indestructible))
			e->indestructible = indestructible;
		if (config_get_uint_option(section, "episode", &value))
			e->episode = value;
		if (config_get_uint_option(section, "killed", &value))
			e->killed = value;
		const char *level;
		if (config_get_string_option(section, "level", &level))
			snprintf(e->level, sizeof(e->level), "%s", level);

		// one of the list: that one is seen
		const int listed = findEnemy(e);
		if (listed < 0)
		{
			++enemyCount;
		}
		else if (enemies[listed].listed && !enemies[listed].seen)
		{
			enemies[listed].seen = true;
			enemies[listed].killed = e->killed;
			enemies[listed].armor = e->armor;
			enemies[listed].indestructible = e->indestructible;
			memcpy(enemies[listed].level, e->level, sizeof(e->level));
			if (e->episode != 0)
				enemies[listed].episode = e->episode;
		}
	}

	config_deinit(&config);
}

static void saveGallery(void)
{
	Config config;
	config_init(&config);

	ConfigSection *section = config_add_section(&config, "items", NULL);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	saveIds(section, "ships", seenShip, COUNTOF(seenShip));
	saveIds(section, "weapons", seenWeapon, COUNTOF(seenWeapon));
	saveIds(section, "sidekicks", seenSidekick, COUNTOF(seenSidekick));
	saveIds(section, "shields", seenShield, COUNTOF(seenShield));
	saveIds(section, "generators", seenGenerator, COUNTOF(seenGenerator));
	saveIds(section, "specials", seenSpecial, COUNTOF(seenSpecial));

	for (unsigned int i = 0; i < enemyCount; ++i)
	{
		const SeenEnemy *e = &enemies[i];
		if (!e->seen)
			continue;

		section = config_add_section(&config, "enemy", NULL);
		if (section == NULL)
			exit(EXIT_FAILURE);  // out of memory

		char parts[MAX_PARTS * 24] = "";
		size_t len = 0;
		for (unsigned int p = 0; p < e->parts; ++p)
			len += (size_t)snprintf(parts + len, sizeof(parts) - len, "%s%u:%u:%u:%d:%d", p > 0 ? " " : "",
			                        e->part[p].bank, e->part[p].gr, e->part[p].size, e->part[p].x, e->part[p].y);
		config_set_string_option(section, "parts", parts);
		config_set_uint_option(section, "armor", e->armor);
		if (e->indestructible)
			config_set_bool_option(section, "indestructible", true, NO_YES);
		config_set_uint_option(section, "episode", e->episode);
		config_set_string_option(section, "level", e->level);
		config_set_uint_option(section, "killed", e->killed);
	}

	File file = userFileOpen(galleryFilename, "w");
	if (file.error)
	{
		logError("Failed to open '%s': %s", galleryFilename, fileGetError(&file));
		config_deinit(&config);
		return;
	}

	config_write(&config, file.f);
	file.error |= ferror(file.f) != 0;

	fileFlush(&file);

	if (file.error)
		logError("Failed to write to '%s': %s", galleryFilename, fileGetError(&file));

	fileClose(&file);
	config_deinit(&config);

	dirty = false;
}

void gallery_level_end(void)
{
	if (recording)
	{
		lookAtPending();
		for (unsigned int t = 0; t < COUNTOF(things); ++t)
			finishThing(t);

		// what was picked up in the level
		markPlayerItems(&player[0].items);
		if (twoPlayerMode)
			markPlayerItems(&player[1].items);
	}
	recording = false;

	if (dirty)
		saveGallery();
}

void gallery_save(void)
{
	if (dirty)
		saveGallery();
}

/* -------- the items there are -------- */

// Every item the upgrade menus of the levels offer (levelsN.dat); the items
// that are not in a menu (secret ships, special weapons) show once seen.
static bool shopShip[SHIP_NUM + 1];
static bool shopFront[PORT_NUM + 1];
static bool shopRear[PORT_NUM + 1];
static bool shopSidekick[OPTION_NUM + 1];
static bool shopShield[SHIELD_NUM + 1];
static bool shopGenerator[POWER_NUM + 1];
static bool shopRead;

static void readShopLists(void)
{
	if (shopRead)
		return;
	shopRead = true;
	const bool wasDirty = dirty;  // MARK sets it; nothing of the player's changes here

	for (int ep = 1; ep <= EPISODE_MAX; ++ep)
	{
		char filename[16];
		snprintf(filename, sizeof(filename), "levels%d.dat", ep);
		if (!dataFileExists(filename))
			continue;

		File file = dataFileOpen(filename, "rb");
		if (file.error)
			continue;

		const long length = fileGetLength(&file);
		char s[256];
		while (!file.error && fileGetPosition(&file) < length)
		{
			readEncryptedString(&file, s, sizeof(s));
			if (file.error || s[0] != ']' || s[1] != 'I')
				continue;

			for (int row = 0; row < 9 && !file.error; ++row)
			{
				readEncryptedString(&file, s, sizeof(s));
				if (file.error)
					break;

				// as JE_loadMap: the numbers after the row's name
				char buf[256];
				snprintf(buf, sizeof(buf), "%s", strlen(s) > 8 ? s + 8 : "");
				int id;
				while (str_pop_int(buf, &id))
				{
					switch (row)
					{
					case 0: MARK(shopShip, id); break;
					case 1: MARK(shopFront, id); break;
					case 2: MARK(shopRear, id); break;
					case 3: MARK(shopGenerator, id); break;
					case 5: case 6: MARK(shopSidekick, id); break;
					case 8: MARK(shopShield, id); break;
					default: break;
					}
				}
			}
		}
		fileClose(&file);
	}
	dirty = wasDirty;
}

/* -------- the screen -------- */

typedef enum
{
	KIND_SHIP,
	KIND_WEAPON,
	KIND_SIDEKICK,
	KIND_SHIELD,
	KIND_GENERATOR,
	KIND_SPECIAL,
	KIND_ENEMY,
} EntryKind;

typedef struct
{
	EntryKind kind;
	unsigned int id;  // item, or index of enemies[]
	bool seen;
} Entry;

#define MAX_ENTRIES (MAX_ENEMIES + 8)

static Entry entries[MAX_ENTRIES];
static unsigned int entryCount;
static unsigned int foundCount;
static unsigned int enemyEpisode;  // the Enemies page shows only this episode's (0: all)
static unsigned int animTick;      // frames since the gallery opened: the enemies' animation

static void addEntry(EntryKind kind, unsigned int id, bool seen)
{
	if (entryCount >= MAX_ENTRIES)
		return;
	entries[entryCount++] = (Entry){ kind, id, seen };
	foundCount += seen;
}

// Whether an item of the data is a real one: the data also has items
// without a picture, "None" and "Test" ones, which no player gets.
static bool realItem(EntryKind kind, unsigned int id)
{
	const char *name;
	unsigned int graphic;
	switch (kind)
	{
	case KIND_WEAPON:    name = weaponPort[id].name; graphic = weaponPort[id].itemgraphic; break;
	case KIND_SIDEKICK:  name = options[id].name;    graphic = options[id].itemgraphic; break;
	case KIND_SHIELD:    name = shields[id].name;    graphic = shields[id].itemgraphic; break;
	case KIND_GENERATOR: name = powerSys[id].name;   graphic = powerSys[id].itemgraphic; break;
	case KIND_SPECIAL:   name = special[id].name;    graphic = special[id].itemgraphic; break;
	default:
		return true;
	}
	if (graphic == 0 || strncmp(name, "Test", 4) == 0)
		return false;

	const char *const none = tr("None");
	return !(strncmp(name, "None", 4) == 0 && (name[4] == ' ' || name[4] == '\0')) &&
	       !(strncmp(name, none, strlen(none)) == 0 && (name[strlen(none)] == ' ' || name[strlen(none)] == '\0'));
}

static void addItems(EntryKind kind, const bool *inShop, const bool *inShop2, const bool *seen, size_t count)
{
	for (size_t id = 1; id < count; ++id)
	{
		const bool offered = (inShop != NULL && inShop[id]) || (inShop2 != NULL && inShop2[id]);
		if ((offered || seen[id]) && realItem(kind, (unsigned int)id))
			addEntry(kind, (unsigned int)id, seen[id]);
	}
}

static void buildEntries(GalleryTab tab)
{
	entryCount = 0;
	foundCount = 0;

	switch (tab)
	{
	case GALLERY_SHIPS:
		addItems(KIND_SHIP, shopShip, NULL, seenShip, COUNTOF(seenShip));
		break;
	case GALLERY_WEAPONS:
		addItems(KIND_WEAPON, shopFront, shopRear, seenWeapon, COUNTOF(seenWeapon));
		break;
	case GALLERY_SIDEKICKS:
		addItems(KIND_SIDEKICK, shopSidekick, NULL, seenSidekick, COUNTOF(seenSidekick));
		break;
	case GALLERY_OTHER:
		addItems(KIND_SHIELD, shopShield, NULL, seenShield, COUNTOF(seenShield));
		addItems(KIND_GENERATOR, shopGenerator, NULL, seenGenerator, COUNTOF(seenGenerator));
		addItems(KIND_SPECIAL, NULL, NULL, seenSpecial, COUNTOF(seenSpecial));
		break;
	case GALLERY_ENEMIES:
	{
		// one not in the list that is a piece of another (seen before the
		// rest of it came) is left out
		static bool piece[MAX_ENEMIES];
		for (unsigned int i = 0; i < enemyCount; ++i)
		{
			piece[i] = false;
			for (unsigned int k = 0; k < enemyCount && !piece[i] && !enemies[i].listed; ++k)
				piece[i] = pieceOf(&enemies[i], &enemies[k]);
		}

		// by episode: the list, then the others in the order they were met
		for (unsigned int ep = 0; ep <= 99; ++ep)
			for (unsigned int i = 0; i < enemyCount; ++i)
				if (!piece[i] && (enemies[i].episode == ep || (ep == 99 && enemies[i].episode > 99)) &&
				    (enemyEpisode == 0 || enemies[i].episode == enemyEpisode))
					addEntry(KIND_ENEMY, i, enemies[i].seen);
		break;
	}
	default:
		break;
	}
}

unsigned int gallery_seen_count(GalleryTab tab)
{
	buildEntries(tab);
	return foundCount;
}

static const char *const tabNames[GALLERY_TABS] = { "Ships", "Weapons", "Sidekicks", "Other", "Enemies", "Statistics" };

// sprite sheets of the enemy banks, loaded when shown
static Sprite2_array bankSheets[COUNTOF(shapeFile) + 1];

static const Sprite2_array *bankSheet(unsigned int bank)
{
	if (bank < 1 || bank > COUNTOF(shapeFile))
		return NULL;

	Sprite2_array *sheet = &bankSheets[bank];
	if (sheet->data == NULL)
	{
		char filename[16];
		snprintf(filename, sizeof(filename), "newsh%c.shp", tolower((unsigned char)shapeFile[bank - 1]));
		if (!dataFileExists(filename))
			return NULL;
		JE_loadCompShapes(sheet, shapeFile[bank - 1]);
	}
	return sheet;
}

static void freeBankSheets(void)
{
	for (size_t i = 0; i < COUNTOF(bankSheets); ++i)
		free_sprite2s(&bankSheets[i]);
}

// The picture of an entry, into the canvas.
// The picture of a piece of an enemy `tick` frames into its animation (as
// the levels do it: a picture each frame, all the time or a round when it
// fires; 999 shows nothing).
static unsigned int partPicture(const Part *p, unsigned int tick)
{
	if (p->anim == 0 || p->anim >= COUNTOF(catalogAnims))
		return p->gr;
	const CatalogAnim *a = &catalogAnims[p->anim];
	if (a->count == 0)
		return p->gr;
	unsigned int frame = tick % a->count;
	if (a->when == 2)
		frame = tick % 70 < a->count ? tick % 70 : 0;  // a round now and then
	else if (a->when == 3)
		frame = tick / 35 % a->count;  // each way it faces, for a second
	return catalogFrames[a->first + frame];
}

// An enemy of the list seen from its other sides too (catalogViews): each
// shown in turn, for two seconds.
static const SeenEnemy *enemyView(unsigned int id, unsigned int tick)
{
	if (id >= COUNTOF(catalogEnemies) || !enemies[id].listed)
		return &enemies[id];

	unsigned int views = 1;
	for (size_t k = 0; k < COUNTOF(catalogViews); ++k)
		views += catalogViews[k].enemy == id;
	unsigned int which = tick / 70 % views;
	if (which == 0)
		return &enemies[id];

	static SeenEnemy view;
	for (size_t k = 0; k < COUNTOF(catalogViews); ++k)
	{
		if (catalogViews[k].enemy != id || --which != 0)
			continue;
		const CatalogView *c = &catalogViews[k];
		view.parts = 0;
		for (unsigned int p = 0; p < c->parts && p < MAX_PARTS; ++p)
		{
			const CatalogPart *cp = &catalogParts[c->first + p];
			view.part[view.parts++] = (Part){ .bank = cp->bank, .size = cp->size, .gr = cp->gr, .x = cp->x, .y = cp->y, .anim = cp->anim };
		}
		break;
	}
	return &view;
}

static void drawEntryPicture(const Entry *entry, unsigned int tick)
{
	canvasClear();

	unsigned int graphic = 0;
	switch (entry->kind)
	{
	case KIND_SHIP:
		// two-sprite ships, as the levels draw them (JE_playerMovement)
		if (ships[entry->id].shipgraphic <= 1)
		{
			const bool nort = ships[entry->id].shipgraphic == 1;
			canvasSprite2x2(&spriteSheet9, nort ? 220 : 13, 0, 0, NULL);
			canvasSprite2x2(&spriteSheet9, nort ? 222 : 51, 24, 0, NULL);
		}
		else if (ships[entry->id].shipgraphic > 500)
			canvasSprite2x2(&spriteSheetT2000, ships[entry->id].shipgraphic - 500, 0, 0, NULL);
		else
			canvasSprite2x2(&spriteSheet9, ships[entry->id].shipgraphic, 0, 0, NULL);
		return;
	case KIND_WEAPON:
		graphic = weaponPort[entry->id].itemgraphic;
		break;
	case KIND_SIDEKICK:
		graphic = options[entry->id].itemgraphic;
		break;
	case KIND_SHIELD:
		graphic = shields[entry->id].itemgraphic;
		break;
	case KIND_GENERATOR:
		graphic = powerSys[entry->id].itemgraphic;
		break;
	case KIND_SPECIAL:
		// drawn from the sheet of the HUD, as in JE_inGameDisplays
		if (special[entry->id].itemgraphic > 0)
			canvasSprite2x2(&spriteSheet10, special[entry->id].itemgraphic, 0, 0, NULL);
		return;
	case KIND_ENEMY:
	{
		// an animated one in the place all its pictures take, so it does not
		// move about nor change size
		const SeenEnemy *e = enemyView(entry->id, tick);
		unsigned int rounds = 1;
		for (unsigned int i = 0; i < e->parts; ++i)
			if (e->part[i].anim != 0 && e->part[i].anim < COUNTOF(catalogAnims))
				rounds = MAX(rounds, catalogAnims[e->part[i].anim].count);
		int x1 = INT32_MAX, y1 = INT32_MAX, x2 = INT32_MIN, y2 = INT32_MIN;
		for (unsigned int r = 0; r <= (rounds > 1 ? rounds : 0); ++r)
		{
			// (a picture a second for the ways it faces: partPicture's when 3)
			const unsigned int at = r < rounds && rounds > 1 ? r * 35 : tick;
			canvasClear();
			for (unsigned int i = 0; i < e->parts; ++i)
			{
				const Part *p = &e->part[i];
				const unsigned int gr = partPicture(p, at);
				if (gr != 999 && gr != 0)
					canvasPiece(bankSheet(p->bank), gr, p->size, 40 + p->x, 40 + p->y);
			}
			if (canvas.x2 > canvas.x1)
			{
				x1 = MIN(x1, canvas.x1);
				y1 = MIN(y1, canvas.y1);
				x2 = MAX(x2, canvas.x2);
				y2 = MAX(y2, canvas.y2);
			}
		}
		if (x2 > x1)
		{
			canvas.x1 = x1;
			canvas.y1 = y1;
			canvas.x2 = x2;
			canvas.y2 = y2;
		}
		return;
	}
	}

	if (graphic > 0)
		canvasSprite2x2(&shopSpriteSheet, graphic, 0, 0, NULL);
}

// The canvas fit into a box, centered, at most `maxScale` times bigger;
// `crop`: a long one only fits its width and is cut; `silhouette`: only its
// shape, in one color.
static bool isVowel(char c)
{
	return c != '\0' && strchr("aeiouAEIOU", c) != NULL;
}

static void showCanvas(SDL_Surface *surface, int bx, int by, int bw, int bh, int maxScale, bool crop, bool silhouette)
{
	if (canvas.x2 <= canvas.x1)
		return;

	const int w = canvas.x2 - canvas.x1, h = canvas.y2 - canvas.y1;

	// scale num/den: up by whole steps, or down to fit
	int num = 1, den = 1;
	if (w <= bw && h <= bh)
	{
		// a tiny one (a mine, a spark) bigger, so it does not look empty
		const int most = (w <= 8 && h <= 8) ? 2 * maxScale : maxScale;
		num = MIN(most, MIN(bw / w, bh / h));
		num = MAX(num, 1);
	}
	else
	{
		// shrink: the larger of the two ratios, rounded so it fits
		num = MIN(bw * 64 / w, bh * 64 / h);
		den = 64;

		// a long row or column would be a thin line: fit its width and show
		// its middle (crop)
		if (crop && num * 2 < den && (w >= 3 * h || h >= 3 * w))
			num = MIN(den, MAX(bw * 64 / w, bh * 64 / h));

		if (num < 1)
			num = 1;
	}

	const int ow = w * num / den, oh = h * num / den;
	const int ox = bx + (bw - ow) / 2, oy = by + (bh - oh) / 2;

	Uint8 *pixels = surface->pixels;
	for (int v = 0; v < oh; ++v)
	{
		const int sy = canvas.y1 + v * den / num;
		const int y = oy + v;
		if (y < 0 || y >= surface->h || y < by || y >= by + bh)
			continue;
		for (int u = 0; u < ow; ++u)
		{
			const int sx = canvas.x1 + u * den / num;
			const int x = ox + u;
			if (x < 0 || x >= surface->w || x < bx || x >= bx + bw)
				continue;
			const size_t at = (size_t)sy * CANVAS_W + sx;
			if (!canvas.set[at])
				continue;
			pixels[(size_t)y * surface->pitch + x] = silhouette ? 0x04 : canvas.pixel[at];  // dark gray
		}
	}
}

// The names in the data are padded with spaces, some in the middle too
// ("USP Talon   Light Fighter").
static void trimmedName(char *dst, size_t size, const char *src)
{
	size_t len = 0;
	for (; *src != '\0' && len + 1 < size; ++src)
		if (*src != ' ' || (len > 0 && dst[len - 1] != ' '))
			dst[len++] = *src;
	while (len > 0 && dst[len - 1] == ' ')
		--len;
	dst[len] = '\0';
}

static const char *entryName(const Entry *entry, char *buffer, size_t size)
{
	if (entry->kind == KIND_ENEMY)
		return "";  // the game has no names for them (and ours could be wrong)
	if (!entry->seen)
		return "???";

	switch (entry->kind)
	{
	case KIND_SHIP:      trimmedName(buffer, size, ships[entry->id].name); break;
	case KIND_WEAPON:    trimmedName(buffer, size, weaponPort[entry->id].name); break;
	case KIND_SIDEKICK:  trimmedName(buffer, size, options[entry->id].name); break;
	case KIND_SHIELD:    trimmedName(buffer, size, shields[entry->id].name); break;
	case KIND_GENERATOR: trimmedName(buffer, size, powerSys[entry->id].name); break;
	case KIND_SPECIAL:   trimmedName(buffer, size, special[entry->id].name); break;
	case KIND_ENEMY:
		break;
	}
	return buffer;
}

enum
{
	TAB_Y = 22,
	GRID_X = 8,
	GRID_Y = 34,
	CELL_W = 28,
	CELL_H = 31,
	GRID_COLS = 6,
	GRID_ROWS = 4,
	PANEL_X = 186,
	PANEL_W = 128,
	COUNT_Y = GRID_Y + GRID_ROWS * CELL_H + 4,
	HELP_Y = 190,
	STATS_Y = GRID_Y + 3,  // the lines of the statistics page
	STATS_LINES = 13,
};

// The tabs, spread over the width of the grid and the panel.
static int tabX(int tab)
{
	int width = 0;
	for (int t = 0; t < GALLERY_TABS; ++t)
		width += JE_textWidth(tr(tabNames[t]), FONT_SMALL);
	const int gap = (PANEL_X + PANEL_W - GRID_X - width) / (GALLERY_TABS - 1);

	int x = GRID_X;
	for (int t = 0; t < tab; ++t)
		x += JE_textWidth(tr(tabNames[t]), FONT_SMALL) + gap;
	return x;
}

// A line of the panel, cut with "..." if it does not fit.
static void drawInfoLine(int *y, const char *text, int value)
{
	char line[96];
	snprintf(line, sizeof(line), "%s", text);
	const int maxW = PANEL_W - 6;
	for (size_t len = strlen(line); len > 3 && JE_textWidth(line, FONT_SMALL) > maxW; --len)
		memcpy(line + len - 4, "...", 4);

	drawFontHvShadow(VGAScreen, PANEL_X + 4, *y, line, FONT_SMALL, 15, value, false, 1);
	*y += 9;
}

static void drawDetails(const Entry *entry)
{
	char buffer[64], line[96];
	const char *const name = entryName(entry, buffer, sizeof(buffer));

	// the name, on more lines if it is long (up to three), in the small font
	// only if it does not fit so
	int y = GRID_Y + 2;
	const int maxW = PANEL_W - 8;
	const Font fonts[2] = { FONT_NORMAL, FONT_SMALL };
	for (int f = 0; f < 2 && name[0] != '\0'; ++f)
	{
		// each line: up to the last space (or after the last hyphen) that fits
		char lines[3][64];
		int count = 0;
		const char *rest = name;
		while (*rest != '\0' && count < 3)
		{
			int cut = -1, next = 0;
			for (int i = 0; rest[i] != '\0' && i < 63; ++i)
			{
				const bool end = rest[i + 1] == '\0';
				if (rest[i] != ' ' && rest[i] != '-' && !end)
					continue;
				const int len = rest[i] == ' ' ? i : i + 1;
				snprintf(lines[count], sizeof(lines[count]), "%.*s", len, rest);
				if (JE_textWidth(lines[count], fonts[f]) > maxW)
					break;
				cut = len;
				next = i + 1;
			}
			if (cut < 0)
			{
				// one word longer than the line: cut it with a hyphen, between
				// syllables if it can (vowel, consonant-vowel: "Invulnera-bilidade")
				int k = 0, any = 0;
				for (int n = 3; rest[n] != '\0' && rest[n] != ' ' && n < 60; ++n)
				{
					if (strlen(rest + n) < 4)
						break;
					snprintf(lines[count], sizeof(lines[count]), "%.*s-", n, rest);
					if (JE_textWidth(lines[count], fonts[f]) > maxW)
						break;
					any = n;
					if (isVowel(rest[n - 1]) && !isVowel(rest[n]) && isalpha((unsigned char)rest[n]) && isVowel(rest[n + 1]))
						k = n;
				}
				if (k == 0)
					k = any;
				if (k == 0)
					break;
				snprintf(lines[count], sizeof(lines[count]), "%.*s-", k, rest);
				++count;
				rest += k;
				continue;
			}
			snprintf(lines[count], sizeof(lines[count]), "%.*s", cut, rest);
			++count;
			rest += next;
		}
		if (*rest != '\0' && f == 0)
			continue;  // does not fit: the small font
		if (count == 0)
			snprintf(lines[count++], sizeof(lines[0]), "%s", name);

		const int brightness = fonts[f] == FONT_NORMAL ? 2 : 4, height = fonts[f] == FONT_NORMAL ? 12 : 9;
		for (int l = 0; l < count; ++l)
			drawFontHvShadow(VGAScreen, PANEL_X + 4, y + l * height, lines[l], fonts[f], 15, brightness, false, 1);
		y += MAX(count, 1) * height;
		break;
	}

	// the enemies: a list put together by hand from the levels
	if (entry->kind == KIND_ENEMY)
	{
		drawInfoLine(&y, tr("List made by fans:"), 0);
		drawInfoLine(&y, tr("it may have mistakes."), 0);
	}

	// the picture, down to where the numbers start
	const int picY = y, infoY = GRID_Y + 2 + 19 + 84 + 3;
	drawEntryPicture(entry, animTick);
	showCanvas(VGAScreen, PANEL_X + 2, picY, PANEL_W - 4, infoY - 3 - picY, 3, false, !entry->seen);
	y = infoY;

	if (!entry->seen)
	{
		drawInfoLine(&y, tr("Not found yet."), 0);
		return;
	}

	switch (entry->kind)
	{
	case KIND_SHIP:
		snprintf(line, sizeof(line), tr("Armor %u"), shop_item_stat(2, entry->id, 1));
		drawInfoLine(&y, line, 2);
		if (ships[entry->id].cost > 0)
		{
			snprintf(line, sizeof(line), tr("Price %u"), (unsigned int)ships[entry->id].cost);
			drawInfoLine(&y, line, 2);
		}
		break;
	case KIND_WEAPON:
		// (one no menu offers, such as the Hot Dogs: which side is not known)
		if (shopFront[entry->id] || shopRear[entry->id])
			drawInfoLine(&y, shopFront[entry->id] && shopRear[entry->id] ? tr("Front or rear weapon")
			                 : shopRear[entry->id] ? tr("Rear weapon") : tr("Front weapon"), 0);
		snprintf(line, sizeof(line), tr("Damage %u at power 1"), shop_item_stat(3, entry->id, 1));
		drawInfoLine(&y, line, 2);
		snprintf(line, sizeof(line), tr("Damage %u at power 11"), shop_item_stat(3, entry->id, 11));
		drawInfoLine(&y, line, 2);
		if (weaponPort[entry->id].cost > 0)
		{
			snprintf(line, sizeof(line), tr("Price %u"), (unsigned int)weaponPort[entry->id].cost);
			drawInfoLine(&y, line, 2);
		}
		break;
	case KIND_SIDEKICK:
		snprintf(line, sizeof(line), tr("Damage %u"), shop_item_stat(7, entry->id, 1));
		drawInfoLine(&y, line, 2);
		if (options[entry->id].cost > 0)
		{
			snprintf(line, sizeof(line), tr("Price %u"), (unsigned int)options[entry->id].cost);
			drawInfoLine(&y, line, 2);
		}
		break;
	case KIND_SHIELD:
		drawInfoLine(&y, tr("Shield"), 0);
		snprintf(line, sizeof(line), tr("Strength %u"), shop_item_stat(5, entry->id, 1));
		drawInfoLine(&y, line, 2);
		if (shields[entry->id].cost > 0)
		{
			snprintf(line, sizeof(line), tr("Price %u"), (unsigned int)shields[entry->id].cost);
			drawInfoLine(&y, line, 2);
		}
		break;
	case KIND_GENERATOR:
		drawInfoLine(&y, tr("Generator"), 0);
		snprintf(line, sizeof(line), tr("Power %u"), shop_item_stat(6, entry->id, 1));
		drawInfoLine(&y, line, 2);
		if (powerSys[entry->id].cost > 0)
		{
			snprintf(line, sizeof(line), tr("Price %u"), (unsigned int)powerSys[entry->id].cost);
			drawInfoLine(&y, line, 2);
		}
		break;
	case KIND_SPECIAL:
		drawInfoLine(&y, tr("Special weapon"), 0);
		break;
	case KIND_ENEMY:
	{
		const SeenEnemy *e = &enemies[entry->id];
		if (e->indestructible && e->armor == 0)
			snprintf(line, sizeof(line), "%s", tr("Indestructible"));
		else
			snprintf(line, sizeof(line), tr("Armor: %u"), e->armor);
		drawInfoLine(&y, line, 2);
		snprintf(line, sizeof(line), tr("Destroyed: %u"), e->killed);
		drawInfoLine(&y, line, 2);
		if (e->episode >= 1 && e->episode < COUNTOF(episode_name) && episode_name[e->episode][0] != '\0')
		{
			char episode[40];
			trimmedName(episode, sizeof(episode), episode_name[e->episode]);
			char *colon = strchr(episode, ':');
			if (colon != NULL && JE_textWidth(episode, FONT_SMALL) > PANEL_W - 6)
				*colon = '\0';  // too long: "Episode 5" only
			drawInfoLine(&y, episode, 0);
		}
		if (e->level[0] != '\0')
		{
			snprintf(line, sizeof(line), tr("Met in %s"), e->level);
			drawInfoLine(&y, line, 0);
		}
		break;
	}
	}
}

/* -------- the view: an entry seen moving -------- */

// In the view the grid's place is a stage: an enemy animated, a ship flying,
// a weapon or a sidekick firing, as the ship menu shows them
// (JE_weaponViewFrame).  The shots are made by the game's own code, on a
// screen of the gallery's (the shots live in the ship menu's box, x 0-140
// and y 0-170 of it), and a piece of that screen is shown.
enum
{
	STAGE_X1 = GRID_X - 3,
	STAGE_Y1 = GRID_Y - 2,
	STAGE_X2 = GRID_X + GRID_COLS * CELL_W + 2,
	STAGE_Y2 = COUNT_Y + 8,
	DEMO_X = 64,   // the ship, on the demo screen
	DEMO_Y = 150,
	DEMO_LEFT = DEMO_X + 7 - (STAGE_X2 - STAGE_X1) / 2,  // shown from here
	DEMO_TOP = DEMO_Y - 110,
	DEMO_STARS = 60,
};

static SDL_Surface *demoScreen;
static SDL_Surface *stageDrawn, *stageBetween;  // with smooth motion (stageShowBetween)
static unsigned int stageTick;
static Player demoSavedPlayer;
static uint demoSavedPower;
static JE_byte demoSavedRepeat[11], demoSavedMulti[11];
static int demoStar[DEMO_STARS][3];  // x, y, speed

static bool demoStart(void)
{
	if (demoScreen == NULL)
		demoScreen = SDL_CreateRGBSurface(0, VGAScreen->w, VGAScreen->h, 8, 0, 0, 0, 0);
	if (stageDrawn == NULL)
		stageDrawn = SDL_CreateRGBSurface(0, VGAScreen->w, VGAScreen->h, 8, 0, 0, 0, 0);
	if (stageBetween == NULL)
		stageBetween = SDL_CreateRGBSurface(0, VGAScreen->w, VGAScreen->h, 8, 0, 0, 0, 0);
	if (demoScreen == NULL || stageDrawn == NULL || stageBetween == NULL)
		return false;

	demoSavedPlayer = player[0];
	demoSavedPower = power;
	memcpy(demoSavedRepeat, shotRepeat, sizeof(shotRepeat));
	memcpy(demoSavedMulti, shotMultiPos, sizeof(shotMultiPos));

	// stars of its own (the game's use mt_rand)
	Uint32 seed = 12345;
	for (int i = 0; i < DEMO_STARS; ++i)
	{
		for (int k = 0; k < 3; ++k)
		{
			seed = seed * 1103515245u + 12345u;
			demoStar[i][k] = (int)((seed >> 8) % (k == 0 ? (unsigned)(STAGE_X2 - STAGE_X1) : k == 1 ? (unsigned)(STAGE_Y2 - STAGE_Y1) : 3u));
		}
		demoStar[i][2] += 1;
	}
	return true;
}

// a new entry or power: no shots of the one before
static void demoReset(void)
{
	memset(shotAvail, 0, sizeof(shotAvail));
	memset(shotRepeat, 1, sizeof(shotRepeat));
	memset(shotMultiPos, 0, sizeof(shotMultiPos));
}

static void demoEnd(void)
{
	demoReset();
	player[0] = demoSavedPlayer;
	power = demoSavedPower;
	memcpy(shotRepeat, demoSavedRepeat, sizeof(shotRepeat));
	memcpy(shotMultiPos, demoSavedMulti, sizeof(shotMultiPos));
	memset(soundQueue, 0, sizeof(soundQueue));
}

// The ship `ship` at (x, y) as the levels draw it (JE_playerMovement),
// banked -2..2.
static void demoDrawShip(SDL_Surface *surface, unsigned int ship, int x, int y, int banking)
{
	const int graphic = ships[ship].shipgraphic;
	if (graphic == 0)
	{
		blit_sprite2x2(surface, x - 17, y - 7, spriteSheet9, 13 + banking * 2);
		blit_sprite2x2(surface, x + 7, y - 7, spriteSheet9, 51 + banking * 2);
	}
	else if (graphic == 1)
	{
		blit_sprite2x2(surface, x - 17, y - 7, spriteSheet9, 220);
		blit_sprite2x2(surface, x + 7, y - 7, spriteSheet9, 222);
	}
	else if (graphic > 500)
	{
		blit_sprite2x2(surface, x - 5, y - 7, spriteSheetT2000, graphic - 500 + banking * 2);
	}
	else
	{
		blit_sprite2x2(surface, x - 5, y - 7, spriteSheet9, graphic + banking * 2);
	}
}

// One frame of the stage for `entry`, `tick` frames since it was chosen;
// weapons at `level` 1..11, in `mode` (rear weapons with two).
// The stage on `dst`: the stars as at `tick` and alpha (0..65536) of the way
// from the frame before, and the demo screen over them.
static void stageCompose(SDL_Surface *dst, const SDL_Surface *demo, unsigned int tick, Uint32 alpha)
{
	const int w = STAGE_X2 - STAGE_X1, h = STAGE_Y2 - STAGE_Y1;

	fill_rectangle_xy(dst, STAGE_X1, STAGE_Y1, STAGE_X2 - 1, STAGE_Y2 - 1, 0);
	const Uint64 at = tick > 0 ? (Uint64)(tick - 1) * 65536 + alpha : 0;
	for (int i = 0; i < DEMO_STARS; ++i)
	{
		const int y = (int)(((Uint64)demoStar[i][1] * 65536 + at * (unsigned)demoStar[i][2]) / 65536 % (unsigned)h);
		((Uint8 *)dst->pixels)[(STAGE_Y1 + y) * dst->pitch + STAGE_X1 + demoStar[i][0]] = demoStar[i][2] == 3 ? 0x0e : demoStar[i][2] == 2 ? 0x0a : 0x07;
	}

	for (int v = 0; v < h; ++v)
	{
		const int sy = DEMO_TOP + v;
		if (sy < 0 || sy >= demo->h)
			continue;
		const Uint8 *src = (const Uint8 *)demo->pixels + sy * demo->pitch;
		Uint8 *out = (Uint8 *)dst->pixels + (STAGE_Y1 + v) * dst->pitch + STAGE_X1;
		for (int u = 0; u < w; ++u)
		{
			const int sx = DEMO_LEFT + u;
			if (sx >= 0 && sx < demo->w && src[sx] != 0)
				out[u] = src[sx];
		}
	}
}

// Smooth motion: a picture in between of the stage; what was drawn over the
// stage after it (the power, the frame) stays
static void stageShowBetween(Uint32 alpha, const SDL_Surface *between, SDL_Surface *out, void *data)
{
	(void)data;
	stageCompose(stageBetween, between, stageTick, alpha);
	for (int y = STAGE_Y1; y < STAGE_Y2; ++y)
	{
		const Uint8 *drawn = (const Uint8 *)stageDrawn->pixels + y * stageDrawn->pitch;
		const Uint8 *src = (const Uint8 *)stageBetween->pixels + y * stageBetween->pitch;
		Uint8 *dst = (Uint8 *)out->pixels + y * out->pitch;
		for (int x = STAGE_X1; x < STAGE_X2; ++x)
			if (dst[x] == drawn[x])
				dst[x] = src[x];
	}
}

static void demoFrame(const Entry *entry, unsigned int tick, unsigned int level, unsigned int mode)
{
	// a ship that sways, the others are shown by the USP Talon
	const unsigned int ship = entry->kind == KIND_SHIP ? entry->id : 1;
	const int sway = entry->kind == KIND_SHIP ? 40 : 0;
	static const Sint8 wave[16] = { 0, 4, 7, 9, 10, 9, 7, 4, 0, -4, -7, -9, -10, -9, -7, -4 };
	const unsigned int phase = tick / 6;
	// between two points of the wave a bit each frame (smooth motion slides it)
	const int step = (int)(tick % 6), at = wave[phase % 16] * (6 - step) + wave[(phase + 1) % 16] * step;
	const int x = DEMO_X + (at * sway + (at < 0 ? -30 : 30)) / 60;
	const int banking = sway == 0 ? 0 : wave[(phase + 4) % 16] / 5;

	SDL_Surface *const screen = VGAScreen;
	SDL_FillRect(demoScreen, NULL, 0);
	VGAScreen = demoScreen;
	smooth_sim_begin_on(demoScreen);

	player[0].x = x;
	player[0].y = DEMO_Y;
	player[0].mouseX = x;
	player[0].mouseY = DEMO_Y;
	player[0].delta_x_shot_move = 0;
	player[0].delta_y_shot_move = 0;
	player[0].last_x_explosion_follow = x;
	player[0].last_y_explosion_follow = DEMO_Y;
	power = 900;

	if (entry->kind == KIND_WEAPON)
	{
		const unsigned int bay = shopRear[entry->id] && !shopFront[entry->id] ? REAR_WEAPON : FRONT_WEAPON;
		if (shotRepeat[bay] > 0)
		{
			--shotRepeat[bay];
		}
		else
		{
			const unsigned int m = weaponPort[entry->id].opnum >= 2 ? mode : 0;
			player_shot_create(entry->id, bay, player[0].x, player[0].y, player[0].mouseX, player[0].mouseY, weaponPort[entry->id].op[m][level - 1], 1);
		}
	}
	else if (entry->kind == KIND_SIDEKICK && options[entry->id].wport > 0)
	{
		for (unsigned int i = 0; i < 2; ++i)
		{
			const unsigned int bay = i == 0 ? SHOT_LEFT_SIDEKICK : SHOT_RIGHT_SIDEKICK;
			if (shotRepeat[bay] > 0)
				--shotRepeat[bay];
			else if (player_shot_create(options[entry->id].wport, bay, x + (i == 0 ? -22 : 24), DEMO_Y + 6, player[0].mouseX, player[0].mouseY, options[entry->id].wpnum, 1) < MAX_PWEAPON &&
			         weapons[options[entry->id].wpnum].sg[0] > 60000)
				shotRepeat[bay] = 40;  // a storm that fills the screen: one now and then
		}
	}
	simulate_player_shots();
	memset(soundQueue, 0, sizeof(soundQueue));
	// the shots it skips: drawn from the sidekick shapes, blended (a storm of
	// plasma), as the level draws them (player_shot_move_and_draw)
	for (int z = 0; z < MAX_PWEAPON - 1; ++z)
	{
		const PlayerShotDataType *shot = &playerShotData[z];
		if (shotAvail[z] == 0)
			continue;
		const unsigned int frame = shot->shotGr + (shot->shotAni > 0 ? shot->shotAni : shot->shotAniMax) - 1;
		if (frame > 60000)
		{
			smooth_tag(SMOOTH_PLAYER_SHOT, (unsigned int)z, 1);
			blit_sprite_blend(demoScreen, shot->shotX + 1, shot->shotY, OPTION_SHAPES, frame - 60001);
		}
	}
	smooth_untag();

	smooth_tag(SMOOTH_PLAYER, 0, 0);
	demoDrawShip(demoScreen, ship, x, DEMO_Y, banking);
	smooth_untag();
	if (entry->kind == KIND_SIDEKICK)
	{
		const JE_OptionType *o = &options[entry->id];
		const unsigned int sprite = o->gr[o->ani > 0 ? (tick / 2) % o->ani : 0];
		for (unsigned int i = 0; i < 2; ++i)
		{
			const int sx = x + (i == 0 ? -22 : 24), sy = DEMO_Y + 6;
			smooth_tag(SMOOTH_SIDEKICK, i, 0);
			if (o->tr == 1 || o->tr == 2)
				blit_sprite2x2(demoScreen, sx - 6, sy, spriteSheet10, sprite);
			else
				blit_sprite2(demoScreen, sx, sy, spriteSheet9, sprite);
		}
	}
	smooth_untag();
	smooth_sim_drawn();
	VGAScreen = screen;

	stageCompose(VGAScreen, demoScreen, tick, 65536);
	stageTick = tick;
}

// A page up (-1) or down: the rows shown move a whole page, so none is shown again
static void turnPage(int *cursor, int *top, int dir)
{
	const int rows = ((int)entryCount + GRID_COLS - 1) / GRID_COLS;
	*top = MAX(0, MIN(*top + dir * GRID_ROWS, rows - GRID_ROWS));
	if (dir < 0)
		*cursor = MAX(*cursor % GRID_COLS, *cursor - GRID_COLS * GRID_ROWS);
	else
		*cursor = MIN((int)entryCount - 1, *cursor + GRID_COLS * GRID_ROWS);
}

static void drawGrid(int top, int cursor)
{
	JE_barShade(VGAScreen, GRID_X - 3, GRID_Y - 2, GRID_X + GRID_COLS * CELL_W + 2, GRID_Y + GRID_ROWS * CELL_H + 1);

	for (int i = 0; i < GRID_COLS * GRID_ROWS; ++i)
	{
		const int index = top * GRID_COLS + i;
		if (index >= (int)entryCount)
			break;

		const int x = GRID_X + (i % GRID_COLS) * CELL_W, y = GRID_Y + (i / GRID_COLS) * CELL_H;
		if (index == cursor)
		{
			fill_rectangle_xy(VGAScreen, x - 1, y - 1, x + CELL_W - 2, y + CELL_H - 2, 0x02);
			JE_rectangle(VGAScreen, x - 1, y - 1, x + CELL_W - 2, y + CELL_H - 2, 0x10 * 15 + 12);
		}

		drawEntryPicture(&entries[index], 0);
		// a tiny enemy twice as big
		showCanvas(VGAScreen, x + 1, y + 1, CELL_W - 4, CELL_H - 4, entries[index].kind == KIND_ENEMY ? 2 : 1, true, !entries[index].seen);
	}

	// more above or below
	const int rows = ((int)entryCount + GRID_COLS - 1) / GRID_COLS;
	if (top > 0)
		drawFontHvShadowAligned(VGAScreen, GRID_X + GRID_COLS * CELL_W - 4, COUNT_Y, "\x18", FONT_SMALL, ALIGN_RIGHT, 15, 4, false, 1);
	if (top + GRID_ROWS < rows)
		drawFontHvShadowAligned(VGAScreen, GRID_X + GRID_COLS * CELL_W + 4, COUNT_Y, "\x19", FONT_SMALL, ALIGN_RIGHT, 15, 4, false, 1);
}

void galleryScreen(void)
{
	if (!ensureEnemies())
		return;

	readShopLists();

	// from the title screen no episode is loaded yet, nor the items
	if (episodeNum == 0)
		JE_loadItemDat();

	JE_loadPic(VGAScreen2, 1, false);  // the ship menu's picture: the items' colors
	JE_barShade(VGAScreen2, 0, 0, 319, 199);
	JE_barShade(VGAScreen2, 0, 0, 319, 199);

	GalleryTab tab = GALLERY_SHIPS;
	int cursor = 0, top = 0;
	bool fadeIn = true;
	bool held[3] = { false, false, false };
	int statsTop = 0, statsCount = 0;
	bool view = false;  // the stage instead of the grid
	unsigned int viewTick = 0, level = 5, mode = 0;
	enemyEpisode = 0;
	animTick = 0;

	buildEntries(tab);

	for (; ; )
	{
		setFrameCount(2);

		memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);

		drawFontHvShadowAligned(VGAScreen, 160, 3, tr("Gallery"), FONT_LARGE, ALIGN_CENTER, 15, -3, false, 2);

		for (int t = 0; t < GALLERY_TABS; ++t)
			drawFontHvShadow(VGAScreen, tabX(t), TAB_Y, tr(tabNames[t]), FONT_SMALL, t == (int)tab ? 15 : 0, t == (int)tab ? 4 : 0, false, 1);
		{
			const int x = tabX(tab), w = JE_textWidth(tr(tabNames[tab]), FONT_SMALL);
			fill_rectangle_xy(VGAScreen, x, TAB_Y + 8, x + w - 1, TAB_Y + 8, 0x10 * 15 + 12);
		}

		if (tab == GALLERY_STATS)
		{
			stats_draw_lines(statsTop, STATS_Y, STATS_LINES);
		}
		else if (view)
		{
			demoFrame(&entries[cursor], viewTick, level, mode);
			JE_rectangle(VGAScreen, STAGE_X1 - 1, STAGE_Y1 - 1, STAGE_X2, STAGE_Y2, 0x10 * 15 + 12);

			char line[64];
			const Entry *e = &entries[cursor];
			if (e->kind == KIND_WEAPON)
			{
				snprintf(line, sizeof(line), tr("Weapon power %u"), level);
				drawFontHvShadow(VGAScreen, STAGE_X1 + 3, STAGE_Y1 + 3, line, FONT_SMALL, 15, 2, false, 1);
				if (weaponPort[e->id].opnum >= 2)
				{
					snprintf(line, sizeof(line), tr("Mode %u"), mode + 1);
					drawFontHvShadow(VGAScreen, STAGE_X1 + 3, STAGE_Y1 + 12, line, FONT_SMALL, 15, 2, false, 1);
				}
			}
			++viewTick;
		}
		else
		{
			drawGrid(top, cursor);

			char line[96];
			if (tab == GALLERY_OTHER)
				snprintf(line, sizeof(line), tr("Found: %u"), foundCount);
			else if (tab == GALLERY_ENEMIES && enemyEpisode != 0)
				snprintf(line, sizeof(line), tr("Episode %u: %u of %u"), enemyEpisode, foundCount, entryCount);
			else
				snprintf(line, sizeof(line), tr("Found: %u of %u"), foundCount, entryCount);
			drawFontHvShadow(VGAScreen, GRID_X, COUNT_Y, line, FONT_SMALL, 15, 2, false, 1);
		}

		if (tab != GALLERY_STATS)
		{

			JE_barShade(VGAScreen, PANEL_X, GRID_Y - 2, PANEL_X + PANEL_W, COUNT_Y + 8);
			if (entryCount > 0)
				drawDetails(&entries[cursor]);
			else
				drawFontHvShadow(VGAScreen, PANEL_X + 4, GRID_Y + 2, tr("Meet them in the levels."), FONT_SMALL, 15, 2, false, 1);
		}

		const int j = joystick_first_gamepad();
		char help[128];
		const bool twoModes = view && entries[cursor].kind == KIND_WEAPON && weaponPort[entries[cursor].id].opnum >= 2;
		if (j >= 0 && view)
		{
			if (entries[cursor].kind == KIND_WEAPON)
				snprintf(help, sizeof(help), twoModes ? tr("Up/Down: power. %s: mode. %s: back.") : tr("Up/Down: power. %s%s: back."),
				         twoModes ? joystick_button_icon(j, SDL_CONTROLLER_BUTTON_Y) : "", joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
			else
				snprintf(help, sizeof(help), tr("Left/Right: the next. %s: back."), joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
		}
		else if (j >= 0 && tab == GALLERY_ENEMIES)
			snprintf(help, sizeof(help), tr("%s%s: page. %s: episode. %s: back."),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_LEFTSHOULDER),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_Y),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
		else if (j >= 0 && tab != GALLERY_STATS && tab != GALLERY_OTHER)
			snprintf(help, sizeof(help), tr("%s%s: page. %s: see it. %s: back."),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_LEFTSHOULDER),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_A),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
		else if (j >= 0)
			snprintf(help, sizeof(help), tr("%s%s: page. %s: back."),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_LEFTSHOULDER),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER),
			         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
		else if (view && entries[cursor].kind == KIND_WEAPON)
			snprintf(help, sizeof(help), "%s", twoModes ? tr("Up/Down: power. Space: mode. Esc: back.") : tr("Up/Down: power. Left/Right: the next. Esc: back."));
		else if (view)
			snprintf(help, sizeof(help), "%s", tr("Left/Right: the next. Esc: back."));
		else if (tab == GALLERY_ENEMIES)
			snprintf(help, sizeof(help), "%s", tr("Tab: page. Arrows: choose. E: episode. Esc: back."));
		else if (tab == GALLERY_STATS)
			snprintf(help, sizeof(help), "%s", tr("Tab: page. Up/Down: scroll. Esc: back."));
		else if (tab == GALLERY_OTHER)
			snprintf(help, sizeof(help), "%s", tr("Tab: page. Arrows: choose. Esc: back."));
		else
			snprintf(help, sizeof(help), "%s", tr("Tab: page. Enter: see it. Esc: back."));
		JE_textShade(VGAScreen, GRID_X, HELP_Y, help, 15, 4, PART_SHADE);

		if (fadeIn)
		{
			fade_palette(colors, 10, 0, 255);
			fadeIn = false;
		}

		// smooth motion: pictures in between of the stage, the rest stays
		if (view)
			SDL_BlitSurface(VGAScreen, NULL, stageDrawn, NULL);
		JE_mouseStart();
		if (!(view && smooth_sim_show_with(2, stageShowBetween, NULL)))
			JE_showVGA();
		JE_mouseReplace();

		waitUntilElapsed();
		++animTick;

		const int oldCursor = cursor, oldStatsTop = statsTop;
		const GalleryTab oldTab = tab;
		int newTab = tab;
		bool done = false;

		bool openView = false, closeView = false, nextEpisode = false;
		int viewStep = 0;

		KeyboardInput keyboardInput;
		while (keyboardGetInput(&keyboardInput))
		{
			if (view)
			{
				// the stage: the next entry, the power and the mode of a weapon
				switch (keyboardInput.scancode)
				{
				case SDL_SCANCODE_ESCAPE:
				case SDL_SCANCODE_RETURN:
					closeView = true;
					break;
				case SDL_SCANCODE_LEFT:
					viewStep = -1;
					break;
				case SDL_SCANCODE_RIGHT:
					viewStep = 1;
					break;
				case SDL_SCANCODE_UP:
					if (level < 11)
						++level;
					demoReset();
					break;
				case SDL_SCANCODE_DOWN:
					if (level > 1)
						--level;
					demoReset();
					break;
				case SDL_SCANCODE_SPACE:
				case SDL_SCANCODE_M:
					mode ^= 1;
					demoReset();
					break;
				case SDL_SCANCODE_TAB:
					closeView = true;
					newTab += (keyboardInput.mod & KMOD_SHIFT) ? -1 : 1;
					break;
				default:
					break;
				}
				continue;
			}

			if (tab != GALLERY_STATS)
			{
				if (keyboardInput.scancode == SDL_SCANCODE_RETURN || keyboardInput.scancode == SDL_SCANCODE_SPACE)
				{
					openView = true;
					continue;
				}
				if (keyboardInput.scancode == SDL_SCANCODE_E && tab == GALLERY_ENEMIES)
				{
					nextEpisode = true;
					continue;
				}
			}

			if (tab == GALLERY_STATS)
			{
				// the statistics scroll
				switch (keyboardInput.scancode)
				{
				case SDL_SCANCODE_UP:
					statsTop -= 1;
					continue;
				case SDL_SCANCODE_DOWN:
					statsTop += 1;
					continue;
				case SDL_SCANCODE_LEFT:
				case SDL_SCANCODE_PAGEUP:
					statsTop -= STATS_LINES - 1;
					continue;
				case SDL_SCANCODE_RIGHT:
				case SDL_SCANCODE_PAGEDOWN:
					statsTop += STATS_LINES - 1;
					continue;
				case SDL_SCANCODE_HOME:
					statsTop = 0;
					continue;
				case SDL_SCANCODE_END:
					statsTop = statsCount;
					continue;
				default:
					break;
				}
			}

			switch (keyboardInput.scancode)
			{
			case SDL_SCANCODE_LEFT:
				if (cursor % GRID_COLS > 0)
					--cursor;
				break;
			case SDL_SCANCODE_RIGHT:
				if (cursor % GRID_COLS < GRID_COLS - 1 && cursor + 1 < (int)entryCount)
					++cursor;
				break;
			case SDL_SCANCODE_UP:
				if (cursor >= GRID_COLS)
					cursor -= GRID_COLS;
				break;
			case SDL_SCANCODE_DOWN:
				if (cursor + GRID_COLS < (int)entryCount)
					cursor += GRID_COLS;
				else if (cursor / GRID_COLS < ((int)entryCount - 1) / GRID_COLS)
					cursor = (int)entryCount - 1;  // the last row is short
				break;
			case SDL_SCANCODE_PAGEUP:
				turnPage(&cursor, &top, -1);
				break;
			case SDL_SCANCODE_PAGEDOWN:
				turnPage(&cursor, &top, 1);
				break;
			case SDL_SCANCODE_HOME:
				cursor = 0;
				break;
			case SDL_SCANCODE_END:
				cursor = (int)entryCount - 1;
				break;
			case SDL_SCANCODE_TAB:
				newTab += (keyboardInput.mod & KMOD_SHIFT) ? -1 : 1;
				break;
			case SDL_SCANCODE_ESCAPE:
				done = true;
				break;
			default:
				break;
			}
		}

		// the controller's shoulder buttons change the page; Y the episode
		// of the enemies, or the mode of a weapon
		const SDL_GameControllerButton buttons[3] = { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_Y };
		for (int s = 0; s < 3; ++s)
		{
			bool down = false;
			for (int k = 0; k < joysticks; ++k)
				if (joystick[k].controller != NULL && SDL_GameControllerGetButton(joystick[k].controller, buttons[s]))
					down = true;
			if (down && !held[s])
			{
				if (s < 2)
				{
					newTab += s == 0 ? -1 : 1;
					closeView = view;
				}
				else if (view)
				{
					mode ^= 1;
					demoReset();
				}
				else if (tab == GALLERY_ENEMIES)
				{
					nextEpisode = true;
				}
			}
			held[s] = down;
		}

		MouseInput mouseInput;
		while (mouseGetInput(INPUT_NO_MOTION, &mouseInput))
		{
			if (mouseInput.button == SDL_BUTTON_RIGHT)
			{
				if (view)
					closeView = true;
				else
					done = true;
			}
			else if (mouseInput.button == SDL_BUTTON_LEFT && view)
			{
				closeView = true;
			}
			else if (mouseInput.button == SDL_BUTTON_LEFT)
			{
				const int mx = mouseInput.x, my = mouseInput.y;
				if (my >= TAB_Y - 2 && my < TAB_Y + 10)
				{
					for (int t = 0; t < GALLERY_TABS; ++t)
						if (mx >= tabX(t) - 3)
							newTab = t;
				}
				else if (tab == GALLERY_STATS)
				{
					// the upper half of the list a page up, the lower half a page down
					if (my < STATS_Y + STATS_LINES * 11 / 2)
						statsTop -= STATS_LINES - 1;
					else
						statsTop += STATS_LINES - 1;
				}
				else if (mx >= GRID_X && mx < GRID_X + GRID_COLS * CELL_W && my >= GRID_Y && my < GRID_Y + GRID_ROWS * CELL_H)
				{
					const int index = (top + (my - GRID_Y) / CELL_H) * GRID_COLS + (mx - GRID_X) / CELL_W;
					if (index == cursor)
						openView = true;  // a click on the one chosen
					else if (index < (int)entryCount)
						cursor = index;
				}
				else if (my >= COUNT_Y - 2 && my < COUNT_Y + 10 && mx >= GRID_X + GRID_COLS * CELL_W - 16 && mx < GRID_X + GRID_COLS * CELL_W + 8)
				{
					// the arrows: a page up or down
					turnPage(&cursor, &top, mx < GRID_X + GRID_COLS * CELL_W - 2 ? -1 : 1);
				}
				else if (my >= COUNT_Y - 2 && my < COUNT_Y + 10 && mx >= GRID_X && tab == GALLERY_ENEMIES)
				{
					nextEpisode = true;  // the count: the next episode
				}
				else if (mx >= PANEL_X && my >= GRID_Y && my < COUNT_Y)
				{
					openView = true;  // the picture
				}
			}
		}

		if (nextEpisode)
		{
			// the next episode that has enemies, then all of them
			const unsigned int was = enemyEpisode;
			do
			{
				enemyEpisode = (enemyEpisode + 1) % 6;
				buildEntries(tab);
			} while (enemyEpisode != 0 && entryCount == 0 && enemyEpisode != was);
			cursor = 0;
			top = 0;
			JE_playSampleNum(S_CLICK);
		}
		if (view && (closeView || done))
		{
			view = false;
			demoEnd();
			done = false;  // Esc closes the stage, not the gallery
			if (closeView)
				JE_playSampleNum(S_SPRING);
		}
		else if (view && viewStep != 0)
		{
			int next = cursor + viewStep;
			while (next >= 0 && next < (int)entryCount && !entries[next].seen)
				next += viewStep;  // what is not found yet stays a shadow
			if (next >= 0 && next < (int)entryCount)
			{
				cursor = next;
				viewTick = 0;
				mode = 0;
				demoReset();
			}
		}
		else if (openView && !view && entryCount > 0 && entries[cursor].seen && tab != GALLERY_OTHER && tab != GALLERY_ENEMIES && demoStart())
		{
			view = true;
			viewTick = 0;
			mode = 0;
			demoReset();
			JE_playSampleNum(S_CLICK);
		}

		newTab = (newTab + GALLERY_TABS) % GALLERY_TABS;
		if (newTab != (int)oldTab)
		{
			tab = (GalleryTab)newTab;
			buildEntries(tab);
			cursor = 0;
			top = 0;
			if (tab == GALLERY_STATS)
				statsCount = stats_build_lines();
			statsTop = 0;
			JE_playSampleNum(S_CLICK);
		}
		else
		{
			statsTop = MAX(0, MIN(statsTop, statsCount - STATS_LINES));
			if (cursor != oldCursor || statsTop != oldStatsTop)
				JE_playSampleNum(S_CURSOR);
		}

		cursor = MAX(0, MIN(cursor, (int)entryCount - 1));
		if (cursor / GRID_COLS < top)
			top = cursor / GRID_COLS;
		else if (cursor / GRID_COLS >= top + GRID_ROWS)
			top = cursor / GRID_COLS - GRID_ROWS + 1;

		if (done)
		{
			JE_playSampleNum(S_SPRING);
			break;
		}
	}

	if (demoScreen != NULL)
	{
		SDL_FreeSurface(demoScreen);
		demoScreen = NULL;
	}
	SDL_FreeSurface(stageDrawn);
	stageDrawn = NULL;
	SDL_FreeSurface(stageBetween);
	stageBetween = NULL;
	freeBankSheets();
}
